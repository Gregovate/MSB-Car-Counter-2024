#include "traffic/Firmware.h"
#include "traffic/JournalInspection.h"
#include "traffic/Esp32JournalPartition.h"
#include "traffic/JournalStore.h"
#include <esp_system.h>
#include <atomic>
#include <algorithm>

namespace {
constexpr uint32_t inspectionBytes=2*traffic::journalBankBytes;
constexpr uint32_t blockBytes=4096;
enum InspectionStage {Waiting,Copying,Verifying,Complete,Failed};
std::atomic<unsigned> inspectionStage{Waiting};
std::atomic<uint32_t> progress{0};
std::atomic<const char*> inspectionFailure{"NONE"};
traffic::Esp32JournalPartition inspectionPartition;
bool inspectionLayout=false,attempted=false;
uint32_t backupHash=2166136261U;
uint8_t sourceBlock[blockBytes],verifyBlock[blockBytes];
File inspectionFile;
String inspectionFolder,backupPath,pendingPath;
const char* stageText(unsigned stage) {
    switch(stage){case Waiting:return "WAITING";case Copying:return "COPYING";
    case Verifying:return "VERIFYING";case Complete:return "VERIFIED";default:return "FAILED";}
}
uint32_t extendHash(uint32_t hash,const uint8_t* data,size_t bytes) {
    for(size_t i=0;i<bytes;++i)hash=(hash^data[i])*16777619U;
    return hash;
}
String hexBytes(const uint8_t* data,size_t bytes) {
    String out;out.reserve(bytes*2);
    for(size_t i=0;i<bytes;++i){char b[3];snprintf(b,sizeof(b),"%02X",data[i]);out+=b;}
    return out;
}
void failInspection(const char* reason) {
    inspectionFailure.store(reason,std::memory_order_release);
    inspectionFile.close();inspectionStage.store(Failed,std::memory_order_release);
    Serial.println("Journal inspection SD backup failed; internal bytes unchanged.");
}
String inspectionJson() {
    auto* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
    auto* a=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,nullptr);
    auto* b=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,nullptr);
    unsigned stage=inspectionStage.load(std::memory_order_acquire);
    char metadata[768];
    snprintf(metadata,sizeof(metadata),
        "{\"firmware\":\"%s\",\"device\":\"%s\",\"journalHealth\":\"%s\","
        "\"layoutApproved\":%s,\"dataAddress\":%lu,\"dataBytes\":%lu,"
        "\"ota0Address\":%lu,\"ota0Bytes\":%lu,\"ota1Address\":%lu,\"ota1Bytes\":%lu,"
        "\"inspectedBytes\":%lu,\"backupStatus\":\"%s\",\"backupProgress\":%lu,"
        "\"backupFnv32\":\"%08lX\",\"backupError\":\"%s\",\"enter\":%d,\"show\":%d,\"banks\":[",
        FWVersion,THIS_MQTT_CLIENT,trafficJournalHealth(),inspectionLayout?"true":"false",
        (unsigned long)(p?p->address:0),(unsigned long)(p?p->size:0),
        (unsigned long)(a?a->address:0),(unsigned long)(a?a->size:0),
        (unsigned long)(b?b->address:0),(unsigned long)(b?b->size:0),
        (unsigned long)inspectionBytes,stageText(stage),(unsigned long)progress.load(),
        (unsigned long)(stage==Complete?backupHash:0),inspectionFailure.load(),totalDailyCars,totalShowCars);
    String json(metadata);
    // A small, local request buffer avoids sharing the backup worker's buffers.
    uint8_t buffer[256];
    for(unsigned bank=0;bank<2;++bank) {
        uint32_t nonErased=0,firstDirty=UINT32_MAX;bool readable=inspectionLayout;
        String header,dirtySample;
        traffic::JournalHeader h{};
        for(uint32_t off=0;readable && off<traffic::journalBankBytes;off+=sizeof(buffer)) {
            if(!inspectionPartition.read(bank*traffic::journalBankBytes+off,buffer,sizeof(buffer))){readable=false;break;}
            if(off==0){memcpy(&h,buffer,sizeof(h));header=hexBytes(buffer,sizeof(h));}
            for(unsigned i=0;i<sizeof(buffer);++i)if(buffer[i]!=0xff){
                ++nonErased;
                if(firstDirty==UINT32_MAX){firstDirty=off+i;dirtySample=hexBytes(buffer+i,std::min(size_t(32),sizeof(buffer)-i));}
            }
        }
        if(bank)json+=",";
        char bankInfo[240];snprintf(bankInfo,sizeof(bankInfo),
            "{\"bank\":%u,\"readable\":%s,\"validJournalHeader\":%s,\"nonErasedBytes\":%lu,\"firstNonErasedOffset\":%ld,\"headerHex\":\"",
            bank,readable?"true":"false",readable && traffic::validJournalHeader(h,bank)?"true":"false",
            (unsigned long)nonErased,firstDirty==UINT32_MAX?-1L:static_cast<long>(firstDirty));
        json+=bankInfo;json+=header;json+="\",\"firstNonErasedHex\":\"";json+=dirtySample;json+="\"}";
    }
    json+="]}";return json;
}
}

void setupJournalInspection() {
    inspectionLayout=inspectionPartition.begin();
    // A new filename per boot preserves evidence, including incomplete earlier
    // backups. This protected SD folder is excluded from TrafficJournal exports.
    inspectionFolder=String("/CC/")+String(recordingYear())+"/TrafficJournalInspection";
    char name[40];snprintf(name,sizeof(name),"/flash-%08lX.bin",(unsigned long)esp_random());
    backupPath=inspectionFolder+name;pendingPath=backupPath+".pending";
    server.on("/diagnostics/journal",HTTP_GET,[](AsyncWebServerRequest* request){
        request->send(200,"application/json",inspectionJson());
    });
    server.on("/diagnostics/journal-backup",HTTP_GET,[](AsyncWebServerRequest* request){
        if(inspectionStage.load(std::memory_order_acquire)!=Complete){request->send(503,"text/plain","Verified backup is not ready");return;}
        // Stream only the verified reserved DATA region. NVS credentials, OTA
        // applications and partition tables are outside this backup range.
        request->send(SD,backupPath,"application/octet-stream",true);
    });
}

void serviceJournalInspection() {
    unsigned stage=inspectionStage.load(std::memory_order_relaxed);
    if(stage==Complete || stage==Failed)return;
    if(stage==Waiting) {
        // No backup writes during healthy journal operation. The evidence stays
        // unchanged; there is deliberately no initialize/erase endpoint here.
        if(attempted || !inspectionLayout || strcmp(trafficJournalHealth(),"FOREIGN_DATA_PRESERVED") || !sdAvailable || !countsReady())return;
        attempted=true;
        if((!SD.exists(inspectionFolder) && !recordingMkdir(inspectionFolder)) || SD.exists(backupPath) || SD.exists(pendingPath)){failInspection("SD_DIRECTORY_OR_NAME_CONFLICT");return;}
        inspectionFile=recordingOpen(pendingPath,FILE_WRITE);
        if(!inspectionFile){failInspection("SD_OPEN_FAILED");return;}
        progress.store(0);backupHash=2166136261U;inspectionStage.store(Copying,std::memory_order_release);
        return;
    }
    uint32_t at=progress.load();
    // At most one 4 KiB source/SD transfer per foreground pass. Detector draining
    // remains between blocks; IRQ capture protects storage stalls as usual.
    if(!inspectionPartition.read(at,sourceBlock,sizeof(sourceBlock))){failInspection("SOURCE_READ_FAILED");return;}
    if(stage==Copying) {
        if(inspectionFile.write(sourceBlock,sizeof(sourceBlock))!=sizeof(sourceBlock)){failInspection("SD_SHORT_WRITE");return;}
        backupHash=extendHash(backupHash,sourceBlock,sizeof(sourceBlock));
    } else {
        if(inspectionFile.read(verifyBlock,sizeof(verifyBlock))!=sizeof(verifyBlock) || memcmp(sourceBlock,verifyBlock,sizeof(sourceBlock))){failInspection("SD_READBACK_MISMATCH");return;}
    }
    at+=blockBytes;progress.store(at);
    if(at<inspectionBytes)return;
    inspectionFile.flush();inspectionFile.close();
    if(stage==Copying) {
        inspectionFile=recordingOpen(pendingPath,FILE_READ);
        if(!inspectionFile || inspectionFile.size()!=inspectionBytes){failInspection("SD_SIZE_OR_OPEN_FAILED");return;}
        progress.store(0);inspectionStage.store(Verifying,std::memory_order_release);return;
    }
    if(!recordingRename(pendingPath,backupPath)){failInspection("SD_RENAME_FAILED");return;}
    inspectionStage.store(Complete,std::memory_order_release);
    Serial.printf("Journal inspection backup verified: %s; %lu bytes; FNV %08lX. Internal flash unchanged.\n",
        backupPath.c_str(),(unsigned long)inspectionBytes,(unsigned long)backupHash);
}
