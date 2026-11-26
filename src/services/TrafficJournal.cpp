#include "traffic/Firmware.h"
#include "traffic/Esp32JournalPartition.h"
#include "traffic/SdJournalArchive.h"
#include <esp_system.h>
#include "traffic/OccupancyPage.h"
#include <Preferences.h>

namespace {
traffic::Esp32JournalPartition partition;
traffic::JournalStore<traffic::Esp32JournalPartition> journal(partition);
bool initialized=false,opened=false,durable=false;
uint32_t archivedThrough=0,lastArchiveMs=0,bootId=0;
traffic::JournalEvent vehicle{};
bool vehiclePending=false;
Preferences sessionPrefs;
enum SessionPhase : uint32_t { Test, Requested, Archived, Ready };
struct SessionReceipt { uint32_t phase, floor, date, checksum; };
SessionReceipt session{};
bool sessionFault=false;
bool rejectedSessionCard=false;
const char* sessionStatus="TEST";

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
    publishMQTT((base+"productionSession").c_str(),sessionStatus,true);
    publishBeamCaptureHealth();
}
void serviceTrafficJournal() {
    if (productionBlocked()) return;
    if(!opened){archiveHealth="FLASH_NOT_READY";return;}
    if(!sdAvailable){archiveHealth="SD_OFFLINE_FLASH_ONLY";return;}
    if(!countsReady()){archiveHealth="WAITING_FOR_BASELINE";return;}
    if(millis()-lastArchiveMs<100)return;
    lastArchiveMs=millis();
    traffic::JournalRecord record{};
    if(!journal.readAfter(archivedThrough,record)){health=storeHealth();return;}
    traffic::SdJournalArchive<fs::SDFS> archive(SD,recordingYear(),root(),session.floor);
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

const char* trafficJournalArchiveHealth(){return archiveHealth;}
uint32_t trafficJournalFreeRecords(){return opened?journal.availableSlots():0;}

// Archive records still present in either flash bank are required by the
// independent bank-reclamation verification. Never remove them, even if old.
bool removeRetiredJournalArchive(const String& path) {
    if(productionActive())return false; // Production records are the source of historical occupancy reports.
    if(!opened || !durable || !sdAvailable || !countsReady() || strcmp(health,"OK") ||
       strcmp(archiveHealth,"OK") || !rtcReady || rtc.lostPower())return false;
    const String prefix=String(seasonFolder)+"/TrafficJournal/";
    if(!path.startsWith(prefix) || path.length()!=prefix.length()+14 || !path.endsWith(".bin"))return false;
    uint64_t parsed=0;
    for(unsigned i=0;i<10;++i){char c=path[prefix.length()+i];if(c<'0'||c>'9')return false;parsed=parsed*10+c-'0';}
    uint32_t oldest=journal.oldestRetainedSequence();
    if(!parsed || parsed>UINT32_MAX || !oldest || parsed>=oldest)return false;
    DateTime now=rtc.now();if(!now.isValid() || now.year()!=recordingYear())return false;
    uint32_t today=now.year()*10000UL+now.month()*100UL+now.day();
    const auto* snapshot=journal.snapshot();
    if(!snapshot || snapshot->date!=today)return false;
    traffic::JournalRecord old{};File f=recordingOpen(path,FILE_READ);
    bool valid=f && f.size()==sizeof(old) && f.read(reinterpret_cast<uint8_t*>(&old),sizeof(old))==sizeof(old);
    f.close();
    if(!valid || !traffic::validJournalRecord(old) || old.sequence!=parsed ||
       (old.event.kind==2 || old.event.kind==7) || !old.counts.date || old.counts.date/10000!=static_cast<uint32_t>(recordingYear()) ||
       old.counts.date>=today)return false;
    // No journal append/erase occurs here. Retirement boundary only advances.
    if(!sdAvailable || journal.health()!=traffic::JournalHealth::Ready ||
       parsed>=journal.oldestRetainedSequence())return false;
    return recordingRemove(path);
}

namespace {
bool validSession(const SessionReceipt& r) {
    return r.phase>=Requested && r.phase<=Ready && r.floor && traffic::countDateValid(r.date) &&
        r.checksum==traffic::countChecksum(r);
}
bool saveSession(SessionReceipt next) {
    next.checksum=traffic::countChecksum(next);
    SessionReceipt check{};
    if(sessionPrefs.putBytes("receipt",&next,sizeof(next))!=sizeof(next) ||
       sessionPrefs.getBytes("receipt",&check,sizeof(check))!=sizeof(check) ||
       memcmp(&next,&check,sizeof(check))) {
        sessionFault=true;sessionStatus="RECEIPT_WRITE_FAILED_REBOOT";return false;
    }
    session=next;return true;
}
}
void initializeProductionSession() {
    if(!sessionPrefs.begin("traffic-session",false)) {
        sessionFault=true;sessionStatus="SESSION_NVS_UNAVAILABLE";return;
    }
    if(!sessionPrefs.isKey("receipt"))return;
    if(sessionPrefs.getBytes("receipt",&session,sizeof(session))!=sizeof(session) || !validSession(session)) {
        sessionFault=true;sessionStatus="INVALID_SESSION_RECEIPT";return;
    }
    sessionStatus=session.phase==Ready?"READY":"RESUMING";
}
uint32_t productionStartDate(){return session.date;}
bool productionRequested(){return !sessionFault && session.phase!=Test && session.date/10000==static_cast<uint32_t>(recordingYear());}
bool productionBlocked(){return sessionFault || (session.phase!=Test && session.phase!=Ready);}
bool productionActive(){return productionRequested() && session.phase==Ready;}

bool requestProductionSession() {
    if(sessionFault)return false;
    if(productionRequested())return true;
    if(!productionDateMatches())return false;
    if(!opened || !journal.snapshot() || !countEventClockValid() || !countsReady() ||
       journal.health()!=traffic::JournalHealth::Ready || !journal.availableSlots()) {
        sessionStatus="REQUEST_REJECTED_CHECK_CLOCK_COUNTS_JOURNAL";return false;
    }
    DateTime now=rtc.now();
    uint32_t date=now.year()*10000UL+now.month()*100UL+now.day();
    archivedThrough=0; // Verify the mounted card from the oldest retained record.
    if(!saveSession({Requested,journal.sequence()+1,date,0}))return false;
    sessionStatus="ARCHIVING_TEST";return true;
}
void serviceProductionSession() {
    if(sessionFault || productionActive())return;
    static uint32_t lastAttempt=0;
    if(millis()-lastAttempt<(productionRequested()?100UL:1000UL))return;
    lastAttempt=millis();
    if(!productionRequested()) {
        if(!productionDateMatches()) {
            sessionStatus=countEventClockValid() && recordingShowStart().unixtime()>rtc.now().unixtime() ? "TEST_WAITING_FOR_START_DATE":"TEST";
            return;
        }
        if(!requestProductionSession())return;
    }
    if(session.date/10000!=static_cast<uint32_t>(recordingYear())){sessionStatus="SESSION_YEAR_CONFLICT";return;}
    if(SD.cardType()==CARD_NONE || !opened){sessionStatus="WAITING_FOR_SD_OR_FLASH";return;}
    String active=String("/")+root()+"/"+String(recordingYear()), archive=active+"-TEST";
    if(session.phase==Requested) {
        // Normal season writes are frozen. The directory rename is the only move.
        // Its two possible final names distinguish a power cut before/after FAT rename.
        if(SD.exists(active)) {
            if(SD.exists(archive)){sessionStatus="TEST_ARCHIVE_NAME_CONFLICT";return;}
            if(!archiveProductionTestFolder(active)){sessionStatus="TEST_ARCHIVE_RENAME_FAILED";return;}
        }
        if(!SD.exists(archive) || SD.exists(active)){sessionStatus="TEST_ARCHIVE_MISSING";return;}
        auto next=session;next.phase=Archived;
        if(!saveSession(next))return;
    }
    if(!SD.exists(archive)){sessionStatus="WAITING_FOR_TEST_CARD";return;}
    if(!SD.exists(active) && !makeProductionFolder(active)){sessionStatus="PRODUCTION_FOLDER_FAILED";return;}
    // Finish copying each retained test record, including flash-only backlog.
    traffic::JournalRecord row{};
    if(journal.readAfter(archivedThrough,row) && row.sequence<session.floor) {
        traffic::SdJournalArchive<fs::SDFS> sink(SD,recordingYear(),root(),session.floor);
        if(!sink.storeVerified(row)){sessionStatus="TEST_ARCHIVE_VERIFY_FAILED";return;}
        archivedThrough=row.sequence;return;
    }
    if(journal.health()!=traffic::JournalHealth::Ready){sessionStatus="JOURNAL_REVIEW_REQUIRED";return;}
    if(journal.sequence()<session.floor) {
        const auto* old=journal.snapshot();
        if(!old || old->sequence==UINT32_MAX){sessionStatus="TEST_BASELINE_INVALID";return;}
        traffic::CountRecord zero{};zero.magic=0x4d534232;zero.sequence=old->sequence+1;
        zero.date=session.date;zero.officialComplete=1;zero.checksum=traffic::countChecksum(zero);
        traffic::JournalEvent marker{};marker.kind=7;marker.bootId=bootId;marker.localEpoch=rtc.now().unixtime();
        if(!officialSeriesCanStart())marker.flags|=4; // Cutover during or after admission.
        if(journal.append(marker,zero)!=traffic::JournalAppend::Durable){sessionStatus="SESSION_MARKER_FAILED";return;}
    }
    auto marker=journal.latest();
    if(!marker || marker->sequence!=session.floor || marker->event.kind!=7){sessionStatus="SESSION_MARKER_CONFLICT";return;}
    // Verify the SD baseline and marker before publishing Ready. Retrying these
    // writes is safe because production vehicles cannot commit in this phase.
    traffic::SdJournalArchive<fs::SDFS> sink(SD,recordingYear(),root(),session.floor);
    if(!sink.storeVerified(*marker)){sessionStatus="SESSION_MARKER_SD_FAILED";return;}
    File f=openProductionBaseline(active,FILE_WRITE);
    size_t n=f?f.write(reinterpret_cast<const uint8_t*>(&marker->counts),sizeof(marker->counts)):0;
    f.flush();f.close();
    traffic::CountRecord check{};
    f=openProductionBaseline(active,FILE_READ);
    bool valid=f && n==sizeof(check) && f.size()==sizeof(check) &&
        f.read(reinterpret_cast<uint8_t*>(&check),sizeof(check))==sizeof(check) &&
        !memcmp(&check,&marker->counts,sizeof(check));
    f.close();
    if(!valid){sessionStatus="SESSION_BASELINE_SD_FAILED";return;}
    auto next=session;next.phase=Ready;
    if(!saveSession(next))return;
    durable=true;health=storeHealth();sdAvailable=true;vehiclePending=false;
    resetSessionRam();applyRecordingCalendar();restoreCountState();
    saveDailyTotal();saveShowTotal();saveHourlyCounts();
    sessionStatus=countsReady()?"READY":"READY_WAITING_FOR_COUNT_RESTORE";publishCountHealth();
}

bool productionCardMatches() {
    if(!productionActive())return true;
    char path[80];
    snprintf(path,sizeof(path),"/%s/%04d/TrafficJournal/%010lu.bin",root(),recordingYear(),(unsigned long)session.floor);
    File f=readProductionCardMarker(path);traffic::JournalRecord marker{};
    bool ok=f && f.size()==sizeof(marker) &&
        f.read(reinterpret_cast<uint8_t*>(&marker),sizeof(marker))==sizeof(marker) &&
        traffic::validJournalRecord(marker) && marker.sequence==session.floor &&
        marker.event.kind==7 && marker.counts.date==session.date;
    f.close();rejectedSessionCard=!ok;
    sessionStatus=ok?"READY":"READY_FLASH_ONLY_CHECK_SESSION_CARD";
    return ok;
}
bool productionCardRejected(){return rejectedSessionCard;}

bool commitClosingCheckpoint() {
    if(!productionActive())return !productionBlocked();
    auto counts=journal.snapshot();
    if(!counts || counts->date!=officialCountDate() || counts->daily!=static_cast<uint32_t>(totalDailyCars) ||
       counts->show!=static_cast<uint32_t>(totalShowCars))return false;
    // Closing metadata is administrative. Never count it as a vehicle.
    traffic::JournalEvent close{};close.kind=8;close.bootId=bootId;close.localEpoch=rtc.now().unixtime();
    close.beforeShow=effectiveShowEnd();close.flags=beamCaptureDropped()?1:0;
    if(sessionPrefs.getBool("clock-gap",false))close.flags|=2;
    auto last=journal.latest();
    if(last && last->event.kind==8 && last->counts.date==counts->date &&
       last->event.beforeShow==close.beforeShow)return true;
    if(journal.append(close,*counts)!=traffic::JournalAppend::Durable)return false;
    if(close.flags&2)sessionPrefs.remove("clock-gap");
    return true;
}

void setupOccupancyRoutes() {
    server.on("/occupancy.html",HTTP_GET,[](AsyncWebServerRequest* request) {
        request->send(request->beginResponse(200,"text/html",occupancyPage));
    });
    server.on("/occupancyJournal",HTTP_GET,[](AsyncWebServerRequest* request) {
        // Serve immutable verified SD records only. The browser combines the two
        // device streams; no network request or full-season scan runs in the detector.
        if(!productionActive() || !sdAvailable) {
            const char* reason=productionActive() ? "SD storage is unavailable." :
                "No active production session. This report uses production show records.";
            auto* response=request->beginResponse(503,"text/plain",reason);
            response->addHeader("Access-Control-Allow-Origin","*");
            request->send(response);
            return;
        }
        if(!request->hasParam("first")) {
            char json[160];snprintf(json,sizeof(json),"{\"floor\":%lu,\"last\":%lu,\"device\":\"%s\"}",
                (unsigned long)session.floor,(unsigned long)archivedThrough,THIS_MQTT_CLIENT);
            auto* response=request->beginResponse(200,"application/json",json);
            response->addHeader("Access-Control-Allow-Origin","*");request->send(response);return;
        }
        uint32_t first=strtoul(request->getParam("first")->value().c_str(),nullptr,10);
        int count=request->hasParam("count")?request->getParam("count")->value().toInt():1;
        if(first<session.floor || count<1 || count>16 || first>archivedThrough ||
           static_cast<uint32_t>(count-1)>archivedThrough-first){request->send(400,"text/plain","Invalid archive range");return;}
        auto* response=request->beginResponseStream("application/octet-stream");
        response->addHeader("Access-Control-Allow-Origin","*");
        for(int i=0;i<count;++i) {
            char path[96];snprintf(path,sizeof(path),"%s/TrafficJournal/%010lu.bin",seasonFolder,(unsigned long)(first+i));
            traffic::JournalRecord record{};File f=recordingOpen(path,FILE_READ);
            bool ok=f && f.size()==sizeof(record) && f.read(reinterpret_cast<uint8_t*>(&record),sizeof(record))==sizeof(record);
            f.close();
            if(!ok || record.sequence!=first+i || !traffic::validJournalRecord(record))break;
            response->write(reinterpret_cast<const uint8_t*>(&record),sizeof(record));
        }
        // A short response is explicitly rejected by the report reader.
        request->send(response);
    });
}

void noteUnqualifiedClock() {
    // No passage row or invented date. Persist one incident until an official
    // close records the uncertainty in the immutable nightly evidence.
    if(!sessionPrefs.getBool("clock-gap",false))sessionPrefs.putBool("clock-gap",true);
}
