#include "traffic/Firmware.h"
#include "traffic/JournalProvisioning.h"
#include "traffic/ProvisioningPolicy.h"
#include "traffic/Esp32JournalPartition.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <atomic>

namespace {
// Operator .20 inspection + downloaded backup SHA256. Never provision arbitrary
// non-journal data, even if a different backup happens to exist on SD.
constexpr char inspectedSha[]="DE4C066D622782D66ECFDC2EA777677CCAC7565BB2272233DE853ED4E48584C1";
constexpr uint32_t provisionBytes=1048576,sectorBytes=4096;
traffic::Esp32JournalPartition provisionPartition;
Preferences provisionPrefs;
bool provisionHandled=false,provisionPrefsOpen=false;
std::atomic<bool> provisionActive{false};
std::atomic<const char*> provisionStatus{"NOT_CHECKED"};
uint8_t provisionOriginal[sectorBytes],provisionObserved[sectorBytes];

void reportProvision(const char* status) {
    provisionStatus.store(status,std::memory_order_release);
    Serial.printf("Journal provisioning: %s\n",status);
    publishJournalProvisioningState();
}
bool exactProvisionLayout() {
    auto* data=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
    auto* a=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,nullptr);
    auto* b=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_1,nullptr);
    auto* running=esp_ota_get_running_partition();
    // Match the reported map AND prove the running application is outside DATA.
    return provisionPartition.begin() && data && a && b && running &&
        data->address==0x290000 && data->size==0x160000 &&
        a->address==0x10000 && a->size==0x140000 && b->address==0x150000 && b->size==0x140000 &&
        running->type==ESP_PARTITION_TYPE_APP && running->size==0x140000 &&
        (running->address==a->address || running->address==b->address);
}
// Verify the actual highest valid SD checkpoint against live RAM immediately
// before destructive work. Never assume a cached OK topic proves a current save.
bool currentSdBaselineVerified(traffic::CountRecord* verified=nullptr) {
    if(!countsReady() || !sdAvailable || !countEventClockValid())return false;
    traffic::CountRecord latest{};
    for(const char* name:{"/CountState.a","/CountState.b"}) {
        File f=recordingOpen(String(seasonFolder)+name,FILE_READ);traffic::CountRecord record{};
        bool valid=f && f.size()==sizeof(record) && f.read(reinterpret_cast<uint8_t*>(&record),sizeof(record))==sizeof(record) && traffic::validCountRecord(record);
        f.close();
        if(valid && latest.sequence && record.sequence==latest.sequence && memcmp(&record,&latest,sizeof(record)))return false;
        if(valid && record.sequence>latest.sequence)latest=record;
    }
    DateTime now=countEventTime();uint32_t date=now.year()*10000UL+now.month()*100UL+now.day();
    if(!latest.sequence || latest.date!=date || latest.daily!=uint32_t(totalDailyCars) || latest.show!=uint32_t(totalShowCars) ||
       latest.officialComplete!=uint32_t(officialCountsComplete) || latest.showResetDate!=(flagDailyShowStartReset?date:0))return false;
    for(unsigned i=0;i<24;++i)if(latest.hourly[i]!=hourlyCount[i] || latest.official[i]!=officialHourly[i])return false;
    if(verified)*verified=latest;
    return true;
}
bool digestInspectedRegion(File* file,char (&out)[65]) {
    mbedtls_sha256_context context;mbedtls_sha256_init(&context);
    bool ok=mbedtls_sha256_starts_ret(&context,0)==0;
    if(file && (!file->seek(0) || file->size()!=provisionBytes))ok=false;
    for(uint32_t at=0;ok && at<provisionBytes;at+=sectorBytes) {
        ok=file?file->read(provisionOriginal,sectorBytes)==sectorBytes:provisionPartition.read(at,provisionOriginal,sectorBytes);
        if(ok)ok=mbedtls_sha256_update_ret(&context,provisionOriginal,sectorBytes)==0;
        detectCar();yield();
    }
    uint8_t digest[32]{};
    if(ok)ok=mbedtls_sha256_finish_ret(&context,digest)==0;
    mbedtls_sha256_free(&context);
    if(ok)for(unsigned i=0;i<32;++i)snprintf(out+2*i,3,"%02X",digest[i]);
    return ok;
}
File findVerifiedInspectionBackup() {
    String folder=String("/CC/")+String(recordingYear())+"/TrafficJournalInspection";
    File dir=recordingOpen(folder,FILE_READ);
    if(!dir || !dir.isDirectory()){dir.close();return File();}
    for(;;) {
        File candidate=dir.openNextFile();if(!candidate)break;
        bool eligible=!candidate.isDirectory() && String(candidate.name()).endsWith(".bin") && candidate.size()==provisionBytes;
        if(eligible) {
            char digest[65]{};
            if(digestInspectedRegion(&candidate,digest) && strcmp(digest,inspectedSha)==0) {dir.close();return candidate;}
        }
        candidate.close();
    }
    dir.close();return File();
}
bool sourceOnlyOriginalOrErased(File& backup,bool& compatible,bool& hasJournal) {
    compatible=true;hasJournal=false;
    if(!backup.seek(0))return false;
    mbedtls_sha256_context context;mbedtls_sha256_init(&context);
    if(mbedtls_sha256_starts_ret(&context,0)!=0){mbedtls_sha256_free(&context);return false;}
    for(uint32_t at=0;at<provisionBytes;at+=sectorBytes) {
        if(backup.read(provisionOriginal,sectorBytes)!=sectorBytes || !provisionPartition.read(at,provisionObserved,sectorBytes) ||
           mbedtls_sha256_update_ret(&context,provisionOriginal,sectorBytes)!=0){mbedtls_sha256_free(&context);return false;}
        if(!traffic::compatibleInterruptedErase(provisionObserved,provisionOriginal,sectorBytes))compatible=false;
        if(at==0 || at==traffic::journalBankBytes) {
            traffic::JournalHeader h{};memcpy(&h,provisionObserved,sizeof(h));
            if(traffic::validJournalHeader(h,at/traffic::journalBankBytes))hasJournal=true;
        }
        detectCar();yield();
    }
    uint8_t digest[32]{};char repeatedSha[65]{};
    bool digestOk=mbedtls_sha256_finish_ret(&context,digest)==0;mbedtls_sha256_free(&context);
    if(!digestOk)return false;
    for(unsigned i=0;i<32;++i)snprintf(repeatedSha+2*i,3,"%02X",digest[i]);
    if(strcmp(repeatedSha,inspectedSha))return false; // Detect deterioration during the comparison pass too.
    // A damaged bank header never authorizes discarding otherwise valid records.
    for(unsigned bank=0;bank<2;++bank)for(uint32_t slot=0;slot<traffic::journalSlots;++slot) {
        traffic::JournalRecord record{};
        if(!provisionPartition.read(bank*traffic::journalBankBytes+traffic::journalHeaderBytes+slot*sizeof(record),&record,sizeof(record)))return false;
        if(traffic::validJournalRecord(record))hasJournal=true;
        if(slot%16==0){detectCar();yield();}
    }
    return true;
}
// If power failed in the very first header write, complete ONLY that known
// header when the entire rest of the owned region is still erased. This never
// erases anything and cannot "repair" old firmware or existing count records.
int repairOwnedEmptyHeader() {
    traffic::JournalHeader expected{};expected.magic=0x4d534248;expected.schema=1;
    expected.bankBytes=traffic::journalBankBytes;expected.recordBytes=sizeof(traffic::JournalRecord);
    expected.checksum=traffic::journalHash(&expected,offsetof(traffic::JournalHeader,checksum));expected.commit=traffic::journalCommit;
    bool changed=false;
    for(uint32_t at=0;at<provisionBytes;at+=sectorBytes) {
        if(!provisionPartition.read(at,provisionObserved,sectorBytes))return -1;
        for(uint32_t i=0;i<sectorBytes;++i) {
            if(at==0 && i<sizeof(expected)) {
                uint8_t want=reinterpret_cast<const uint8_t*>(&expected)[i];
                if((provisionObserved[i]&want)!=want)return 0;
                if(provisionObserved[i]!=0xff)changed=true;
            } else if(provisionObserved[i]!=0xff)return 0;
        }
        detectCar();yield();
    }
    if(!changed)return 0; // Completely erased region follows the ordinary path.
    if(!currentSdBaselineVerified() || !exactProvisionLayout())return -1;
    if(!provisionPartition.write(0,&expected,offsetof(traffic::JournalHeader,commit)) ||
       !provisionPartition.write(offsetof(traffic::JournalHeader,commit),&expected.commit,sizeof(expected.commit)))return -1;
    traffic::JournalHeader check{};
    return provisionPartition.read(0,&check,sizeof(check)) && traffic::validJournalHeader(check,0)?1:-1;
}
bool seedVerifiedCountsAndRestart() {
    detectCar();traffic::CountRecord baseline{};
    if(!currentSdBaselineVerified(&baseline))return false;
    Preferences pending;
    if(!pending.begin("traffic-pending",true))return false;
    uint32_t unassigned=pending.getUInt("vehicles",0);pending.end();
    traffic::JournalStore<traffic::Esp32JournalPartition> first(provisionPartition);
    if(!first.open(true))return false;
    first.carryPending(unassigned);
    traffic::JournalEvent event{};event.kind=3;event.localEpoch=countEventTime().unixtime();event.endUptimeMs=millis();
    if(first.append(event,baseline)!=traffic::JournalAppend::Durable)return false;
    // A durable count snapshot now exists BEFORE restart, so the next boot can
    // recover counts even if this intermittently failing SD card does not mount.
    bool finalized=provisionPrefs.putString("phase","SEEDED")==6 && provisionPrefs.getString("phase","")=="SEEDED";
    reportProvision(finalized?"SEEDED_RESTARTING":"SEEDED_RESTARTING_RECEIPT_PENDING");
    provisionHandled=true;ESP.restart();return true;
}
void finishProvisionAttempt(const char* status) {
    provisionActive.store(false,std::memory_order_release);reportProvision(status);provisionHandled=true;
}
}

void publishJournalProvisioningState(){publishMQTT((String(topic_base_path)+"/System/journalProvisioning").c_str(),journalProvisioningState(),true);}
bool journalProvisioningActive(){return provisionActive.load(std::memory_order_acquire);}
const char* journalProvisioningState(){return provisionStatus.load(std::memory_order_acquire);}
void serviceJournalProvisioning() {
    if(provisionHandled || !countsReady())return;
    if(!provisionPrefsOpen)provisionPrefsOpen=provisionPrefs.begin("traffic-jprov",false);
    if(strcmp(trafficJournalHealth(),"OK")==0) {
        if(provisionPrefsOpen && provisionPrefs.getString("source-sha","")==inspectedSha) {
            // Healthy journals are NEVER provisioned, even if receipt cleanup fails.
            provisionPrefs.putString("phase","DONE");provisionPrefs.remove("source-sha");
        }
        finishProvisionAttempt("READY");return;
    }
    if(!sdAvailable)return;
    if(strcmp(trafficJournalHealth(),"FOREIGN_DATA_PRESERVED") || !exactProvisionLayout()) {
        finishProvisionAttempt("REJECTED_LAYOUT_OR_JOURNAL_STATE");return;
    }
    provisionActive.store(true,std::memory_order_release);reportProvision("VERIFYING_INSPECTED_SOURCE");
    if(!provisionPrefsOpen){finishProvisionAttempt("NVS_RECEIPT_UNAVAILABLE_NO_ERASE");return;}
    char sourceDigest[65]{};
    if(!digestInspectedRegion(nullptr,sourceDigest)){finishProvisionAttempt("SOURCE_READ_FAILED_NO_ERASE");return;}
    bool sourceMatches=strcmp(sourceDigest,inspectedSha)==0;
    String phase=provisionPrefs.getString("phase","");
    bool receipt=provisionPrefs.getString("source-sha","")==inspectedSha && (phase=="ARMED" || phase=="ERASED");
    if(!sourceMatches && !receipt){finishProvisionAttempt("UNRECOGNIZED_DATA_NO_ERASE");return;}
    reportProvision("VERIFYING_PRESERVED_SD_BACKUP");
    File backup=findVerifiedInspectionBackup();
    if(!backup){finishProvisionAttempt("MATCHING_BACKUP_NOT_FOUND_NO_ERASE");return;}
    if(receipt && !sourceMatches) {
        int repaired=repairOwnedEmptyHeader();
        if(repaired!=0) {
            backup.close();
            if(repaired>0 && seedVerifiedCountsAndRestart())return;
            finishProvisionAttempt("OWNED_HEADER_OR_SEED_RECOVERY_FAILED");return;
        }
    }
    bool compatible=false,hasJournal=false;
    bool readOk=sourceOnlyOriginalOrErased(backup,compatible,hasJournal);backup.close();
    if(!readOk || !traffic::allowInspectedProvision(true,true,sourceMatches,receipt,compatible,hasJournal,currentSdBaselineVerified())) {
        finishProvisionAttempt("BACKUP_SOURCE_OR_BASELINE_MISMATCH_NO_ERASE");return;
    }
    // Migration intent is durable BEFORE the first erase. A reset can resume only
    // against the same independently verified backup and monotonic erase pattern.
    if(!receipt) {
        bool saved=provisionPrefs.putString("source-sha",inspectedSha)==64 && provisionPrefs.putString("phase","ARMED")==5;
        if(!saved || provisionPrefs.getString("source-sha","")!=inspectedSha || provisionPrefs.getString("phase","")!="ARMED") {
            finishProvisionAttempt("RECEIPT_SAVE_FAILED_NO_ERASE");return;
        }
    }
    if(!exactProvisionLayout()){finishProvisionAttempt("LAYOUT_CHANGED_NO_ERASE");return;}
    reportProvision("INITIALIZING_VERIFIED_DATA_REGION");
    for(uint32_t at=0;at<provisionBytes;at+=sectorBytes) {
        detectCar();
        if(!currentSdBaselineVerified()){finishProvisionAttempt("PAUSED_SD_BASELINE_RECEIPT_PRESERVED");return;}
        if(!provisionPartition.erase(at,sectorBytes) || !provisionPartition.read(at,provisionObserved,sectorBytes) || !traffic::journalErased(provisionObserved,sectorBytes)) {
            finishProvisionAttempt("ERASE_VERIFY_FAILED_RECEIPT_PRESERVED");return;
        }
        yield();
    }
    // Seed and verify live counts in the newly owned region before rebooting.
    if(!seedVerifiedCountsAndRestart())finishProvisionAttempt("INITIAL_SNAPSHOT_SAVE_FAILED_RECEIPT_PRESERVED");
}
