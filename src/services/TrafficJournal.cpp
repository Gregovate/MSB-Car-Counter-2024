#include "traffic/Firmware.h"
#include "traffic/Esp32JournalPartition.h"
#include "traffic/SdJournalArchive.h"
#include <esp_system.h>

namespace {
traffic::Esp32JournalPartition partition;
traffic::JournalStore<traffic::Esp32JournalPartition> journal(partition);
bool initialized=false,opened=false,durable=false;
uint32_t archivedThrough=0,lastArchiveMs=0,bootId=0;
traffic::JournalEvent vehicle{};
bool vehiclePending=false;
const char* health="NOT_INITIALIZED";
const char* archiveHealth="NOT_ATTEMPTED";
const char* root(){return String(THIS_MQTT_CLIENT)=="espGateCounter" ? "GC":"CC";}
const char* storeHealth() {
    switch(journal.health()) {
    case traffic::JournalHealth::Ready:return "OK";
    case traffic::JournalHealth::ForeignData:return "FOREIGN_DATA_PRESERVED";
    case traffic::JournalHealth::LayoutRejected:return "LAYOUT_REJECTED_OTA_PRESERVED";
    case traffic::JournalHealth::Full:return "FULL_SD_ARCHIVE_REQUIRED";
    case traffic::JournalHealth::Corrupt:return "CORRUPT_PRESERVED";
    case traffic::JournalHealth::ReadFailed:return "FLASH_READ_FAILED";
    case traffic::JournalHealth::WriteFailed:return "FLASH_WRITE_FAILED";
    default:return "NOT_READY";
    }
}
}
void initializeTrafficJournal() {
    if(initialized)return;
    initialized=true;bootId=esp_random();
    opened=journal.open(partition.begin());health=storeHealth();durable=opened && journal.snapshot();
    // No automatic formatting/mounting; unknown existing contents survive.
    Serial.printf("Traffic journal: %s; slots=%lu\n",health,(unsigned long)journal.availableSlots());
}
const traffic::CountRecord* latestTrafficSnapshot(){initializeTrafficJournal();return opened?journal.snapshot():nullptr;}
uint32_t journalPendingVehicles(){initializeTrafficJournal();return opened?journal.pendingCount():0;}
const char* trafficJournalHealth(){return health;}
bool trafficJournalDurable(){return durable;}
void setTrafficVehicleEvent(uint32_t endMs,uint32_t passageMs,uint32_t followMs,uint32_t brokenMs,uint32_t betweenMs,uint32_t clearBeforeMs) {
    vehicle={};vehicle.bootId=bootId;vehicle.kind=1;vehicle.direction=1;
    vehicle.endUptimeMs=endMs;vehicle.passageMs=passageMs;vehicle.followMs=followMs;
    vehicle.bBrokenMs=brokenMs;vehicle.betweenMs=betweenMs;vehicle.clearBeforeMs=clearBeforeMs;
    vehicle.maxSampleGapMs=beamCaptureMaxGapMs();
    vehicle.flags=beamCaptureDropped()?1:0;
    vehicle.beforeDaily=totalDailyCars;vehicle.beforeShow=totalShowCars;
    vehiclePending=false; // Maintenance/reset snapshots must not consume this staged event.
}
bool appendTrafficSnapshot(const traffic::CountRecord& counts) {
    initializeTrafficJournal();
    traffic::JournalEvent e=vehiclePending?vehicle:traffic::JournalEvent{};
    e.bootId=bootId;e.localEpoch=countEventTime().unixtime();
    if(!vehiclePending){e.kind=3;e.endUptimeMs=millis();}
    const auto result=journal.append(e,counts);
    vehiclePending=false;durable=result==traffic::JournalAppend::Durable;health=storeHealth();
    return durable;
}
bool appendUndatedTraffic(uint32_t pending) {
    initializeTrafficJournal();
    traffic::JournalEvent e=vehiclePending?vehicle:traffic::JournalEvent{};
    e.bootId=bootId;e.kind=2;e.localEpoch=countEventClockValid()?countEventTime().unixtime():0;e.beforeDaily=pending;
    // No date or fictitious baseline. Pending aggregate is in this committed event.
    traffic::CountRecord empty{};
    const auto result=journal.append(e,empty);
    vehiclePending=false;durable=result==traffic::JournalAppend::Durable;health=storeHealth();
    return durable;
}
void publishTrafficJournalHealth() {
    const String base=String(topic_base_path)+"/System/";
    publishMQTT((base+"journalHealth").c_str(),health,true);
    publishMQTT((base+"journalArchive").c_str(),archiveHealth,true);
    publishMQTT((base+"journalFreeRecords").c_str(),String(opened?journal.availableSlots():0),true);
    publishBeamCaptureHealth();
}
void serviceTrafficJournal() {
    if(!opened){archiveHealth="FLASH_NOT_READY";return;}
    if(!sdAvailable){archiveHealth="SD_OFFLINE_FLASH_ONLY";return;}
    if(!countsReady()){archiveHealth="WAITING_FOR_BASELINE";return;}
    if(millis()-lastArchiveMs<100)return;
    lastArchiveMs=millis();
    traffic::JournalRecord record{};
    if(!journal.readAfter(archivedThrough,record)){health=storeHealth();return;}
    traffic::SdJournalArchive<fs::SDFS> archive(SD,recordingYear(),root());
    if(!archive.storeVerified(record)){archiveHealth="SD_WRITE_VERIFY_FAILED_RETRY";return;}
    archivedThrough=record.sequence;archiveHealth="OK";
    // Reserve bank reuse for verified archives only. Retain the newest snapshot.
    if(journal.availableSlots()<traffic::journalSlots/2) {
        // Do not rescan hundreds of SD files after EACH backlog record. The
        // cursor is only a scheduling gate; reclaim still independently verifies
        // every record before committing an erase receipt.
        for(unsigned bank=0;bank<2;++bank)
            if(journal.bankLastSequence(bank) && archivedThrough>=journal.bankLastSequence(bank))journal.reclaim(bank,archive);
        health=storeHealth();
    }
}

uint32_t journalCorrectionNonce(){initializeTrafficJournal();return opened?journal.correctionNonce():0;}
void carryJournalPending(uint32_t pending){initializeTrafficJournal();if(opened)journal.carryPending(pending);}
bool appendTrafficCorrection(const traffic::CountRecord& counts,uint32_t nonce) {
    initializeTrafficJournal();
    if(nonce<=journal.correctionNonce())return false;
    traffic::JournalEvent event{};event.kind=6;event.bootId=bootId;
    event.localEpoch=countEventTime().unixtime();event.endUptimeMs=millis();event.correctionEpoch=nonce;
    auto result=journal.append(event,counts);
    durable=result==traffic::JournalAppend::Durable;health=storeHealth();return durable;
}

void activateTrafficVehicleEvent(){vehicle.beforeDaily=totalDailyCars;vehicle.beforeShow=totalShowCars;vehiclePending=true;}
