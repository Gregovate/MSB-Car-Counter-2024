#include "traffic/Firmware.h"
#include "traffic/CountRecord.h"
#include <limits.h>
#include <Preferences.h>

unsigned int officialHourly[24]{};
bool officialCountsComplete=false;

// Identical in both devices. Legacy text/CSV files remain for the web UI;
// CountState.a/b are the authoritative, verified reboot checkpoint from .4 on.
namespace {
traffic::CountRecord savedCounts{};
traffic::CountRecord closedCounts{};
uint32_t countDate=0, showResetDate=0;
int freshCountYear=0;
int countSlot=-1;
bool sdCheckpointKnown=false;
bool countReady=false;
bool retryClockRestore=false;
unsigned long lastRestoreAttempt=0;
// Undated events are durable but never assigned to a fabricated day/hour.
Preferences pendingPrefs;
bool pendingLoaded=false;
uint32_t pendingVehicles=0;
bool pendingDurable=true;
void loadPendingVehicles() {
    if (pendingLoaded) return;
    pendingLoaded=true;
    pendingDurable=pendingPrefs.begin("traffic-pending",false);
    if (pendingDurable) pendingVehicles=pendingPrefs.getUInt("vehicles",0);
    uint32_t journalPending=journalPendingVehicles();
    if(journalPending>pendingVehicles)pendingVehicles=journalPending;
    carryJournalPending(pendingVehicles);
    pendingDurable=pendingDurable || strcmp(trafficJournalHealth(),"OK")==0;
}
const char* countFault="NOT_RESTORED";
String hourlyExportHealth="NOT_ATTEMPTED";

uint32_t countToday() {
    if (!rtcReady || rtc.lostPower()) return 0;
    DateTime now=countEventTime();
    if (!now.isValid() || now.year()!=recordingYear()) return 0;
    return now.year()*10000UL+now.month()*100UL+now.day();
}
String countPath(const String& name) {
    if (name.startsWith(String(seasonFolder)+"/")) return name;
    return String(seasonFolder)+(name.startsWith("/") ? "" : "/")+name;
}
void reportCountFault(const char* fault) {
    if (strcmp(countFault,fault)!=0) {
        Serial.printf("Count persistence: %s\n",fault);
        publishMQTT(MQTT_DEBUG_LOG,String("Count persistence: ")+fault);
    }
    countFault=fault;
    publishMQTT((String(topic_base_path)+"/System/countPersistence").c_str(),fault,true);
}
bool readCountRecord(int slot, traffic::CountRecord& record) {
    File f=recordingOpen(countPath(slot==0 ? "CountState.a" : "CountState.b"),FILE_READ);
    if (!f) return false;
    bool ok=false;
    if (f.size()==sizeof(traffic::LegacyCountRecord)) {
        traffic::LegacyCountRecord old{};
        ok=f.read(reinterpret_cast<uint8_t*>(&old),sizeof(old))==sizeof(old) && traffic::decodeLegacyCount(old,record);
    } else ok=f.size()==sizeof(record) && f.read(reinterpret_cast<uint8_t*>(&record),sizeof(record))==sizeof(record);
    f.close();
    return ok && traffic::validCountRecord(record);
}
bool persistClosedCounts() {
    if (!closedCounts.date) return true;
    String path=countPath(String("ClosedCounts-")+String(closedCounts.date)+".bin");
    File existing=recordingOpen(path,FILE_READ);
    traffic::CountRecord check{};
    bool same=existing && existing.size()==sizeof(check) &&
        existing.read(reinterpret_cast<uint8_t*>(&check),sizeof(check))==sizeof(check) &&
        memcmp(&check,&closedCounts,sizeof(check))==0;
    existing.close();
    if (!same) {
        File f=recordingOpen(path,FILE_WRITE);
        size_t n=f ? f.write(reinterpret_cast<const uint8_t*>(&closedCounts),sizeof(closedCounts)):0;
        f.flush();f.close();
        File verify=recordingOpen(path,FILE_READ);
        same=verify && n==sizeof(check) && verify.size()==sizeof(check) &&
            verify.read(reinterpret_cast<uint8_t*>(&check),sizeof(check))==sizeof(check) &&
            memcmp(&check,&closedCounts,sizeof(check))==0;
        verify.close();
    }
    if (!same) {reportCountFault("CLOSED_DAY_SAVE_FAILED_KEEP_CHECKPOINT");return false;}
    closedCounts={};return true;
}
bool parseCountNumber(const String& text, uint32_t& value, bool allowEmpty=false) {
    const char* p=text.c_str();
    while (*p==' ' || *p=='\r' || *p=='\n' || *p=='\t') ++p;
    if (!*p) { value=0; return allowEmpty; }
    uint32_t n=0;
    if (*p<'0' || *p>'9') return false;
    while (*p>='0' && *p<='9') {
        if (n>(uint32_t(INT_MAX)-uint32_t(*p-'0'))/10) return false;
        n=n*10+(*p++-'0');
    }
    while (*p==' ' || *p=='\r' || *p=='\n' || *p=='\t') ++p;
    if (*p) return false;
    value=n; return true;
}
bool readLegacyCount(const String& path, uint32_t& value, bool allowEmpty=false) {
    File f=recordingOpen(countPath(path),FILE_READ);
    if (!f || f.size()>32) { f.close(); return false; }
    size_t expected=f.size();
    String text=f.readString(); f.close();
    if (text.length()!=expected) return false;
    return parseCountNumber(text,value,allowEmpty);
}
bool loadLegacyCounts(uint32_t today) {
    // One-time migration only from the CURRENT year. Never infer daily totals
    // by summing hourly values: show-time resets give them different meanings.
    uint32_t daily=0, show=0, day=0;
    if (!readLegacyCount(fileName1,daily) || !readLegacyCount(fileName2,show,true) ||
        !readLegacyCount(fileName3,day) || day<1 || day>31) return false;
    File file=recordingOpen(countPath(fileName5),FILE_READ);
    if (!file) return false;
    uint32_t latest=0, hours[24]{}; bool found=false, valid=true;
    while (file.available()) {
        String line=file.readStringUntil('\n'); line.trim();
        if (!line.length() || line.startsWith("Date")) continue;
        int y=0,m=0,d=0,used=0;
        if (sscanf(line.c_str(),"%d-%d-%d,%n",&y,&m,&d,&used)!=3 || !used || !traffic::validDate(y,m,d)) { valid=false; break; }
        uint32_t date=y*10000UL+m*100UL+d;
        if (date/10000!=today/10000 || date>today) { valid=false; break; }
        uint32_t row[24]{};
        for (int i=0;i<24;++i) {
            int comma=line.indexOf(',',used);
            if ((i<23 && comma<0) || (i==23 && comma>=0) ||
                !parseCountNumber(comma<0 ? line.substring(used) : line.substring(used,comma),row[i])) { valid=false; break; }
            used=comma+1;
        }
        if (!valid) break;
        if (date>latest) latest=date;
        if (date==today) { for(int i=0;i<24;++i) hours[i]=row[i]; found=true; }
    }
    file.close();
    if (!valid) return false;
    if (day==today%100) {
        // A nonzero daily total without a dated hourly row is ambiguous.
        if (!found && daily!=0) return false;
    } else {
        // A verified zero daily total can stay zero when all hourly rows are
        // older than today, even if the separate day file advanced later.
        // Legacy firmware produced exactly this split during failed saves.
        // Preserve those older rows; never sum them into today's total.
        // Nonzero totals still require the original day/date consistency check.
        if (!latest || latest>=today || (daily!=0 && latest%100!=day)) return false;
        daily=0;
    }
    totalDailyCars=daily; totalShowCars=show;
    for(int i=0;i<24;++i) hourlyCount[i]=found ? hours[i] : 0;
    countDate=today;
    return true;
}
}

void noteFreshCountFolder() {
    // Called only after successfully creating a previously absent annual folder.
    freshCountYear=recordingYear();
}

void reportHourlyExportStatus(const char* status) {
    // Latch current health separately from the offline event queue. Both devices
    // repeat this value after connection; no old count messages are replayed.
    hourlyExportHealth=status;
    publishMQTT((String(topic_base_path)+"/System/hourlyExport").c_str(),hourlyExportHealth,true);
}

void recordPendingVehicle() {
    loadPendingVehicles();
    if (pendingVehicles<UINT32_MAX) ++pendingVehicles;
    // Preferences commits each accepted event. No invalid RTC date enters SD.
    bool flashSaved=appendUndatedTraffic(pendingVehicles);
    bool nvsSaved=pendingPrefs.putUInt("vehicles",pendingVehicles)==sizeof(uint32_t);
    pendingDurable=flashSaved || nvsSaved;
    publishCountHealth();
}

void publishCountHealth() {
    loadPendingVehicles();
    publishTrafficJournalHealth();
    publishMQTT((String(topic_base_path)+"/System/unassignedRecoveryCount").c_str(),String(pendingVehicles),true);
    publishMQTT((String(topic_base_path)+"/System/pendingCountHealth").c_str(),
        pendingDurable ? (pendingVehicles ? "UNASSIGNED_EVENTS_REQUIRE_REVIEW":"OK") : "SAVE_FAILED_FLASH_AND_NVS",true);
    reportCountFault(countFault);
    publishMQTT((String(topic_base_path)+"/System/hourlyExport").c_str(),hourlyExportHealth,true);
    publishMQTT((String(topic_base_path)+"/System/unrestoredDailyCount").c_str(),String(countReady ? 0 : totalDailyCars),true);
}

bool countsReady() { return countReady; }
bool countEventCanApply(){return countReady && countEventClockValid() && countToday()==countDate;}

bool countTopicPublishable(const char* topic) {
    if (countReady) return true;
    String t(topic), base(topic_base_path);
    // Never replace retained counts/heartbeat with startup defaults after a
    // recovery failure. Health/config/alarms continue to publish normally.
    return !t.startsWith(base+"/Cars/") && t!=base+"/System/heartbeat";
}

bool restoreCountState() {
    if (countReady) return true; // A retry must never replace live totals.
    loadPendingVehicles();
    uint32_t today=countToday();
    if (!today) {
        retryClockRestore=true;
        reportCountFault("WAITING_FOR_VALID_CLOCK"); return false;
    }
    retryClockRestore=false;
    // Internal flash is independently authoritative when SD is absent. Compare
    // CountRecord sequences in one lineage; journal event sequence is unrelated.
    traffic::CountRecord a{},b{};
    bool va=false,vb=false;
    const traffic::CountRecord* flash=latestTrafficSnapshot();
    if(SD.cardType()!=CARD_NONE){va=readCountRecord(0,a);vb=readCountRecord(1,b);}
    sdCheckpointKnown=va || vb;
    if(flash) {
        traffic::CountRecord sd{};
        if(va || vb)sd=vb && (!va || b.sequence>a.sequence)?b:a;
        if(sd.sequence==flash->sequence && memcmp(&sd,flash,sizeof(sd))) {
            reportCountFault("RESTORE_CONFLICT_KEEP_RECORDS");return false;
        }
        if(flash->sequence>=sd.sequence){sdCheckpointKnown=sd.sequence==flash->sequence;a=*flash;va=true;vb=false;}
    }
    if(!va && !vb && SD.cardType()==CARD_NONE) {
        retryClockRestore=true;reportCountFault("RESTORE_FAILED_CLOCK_OR_SD");return false;
    }
    if (va || vb) {
        countSlot=vb && (!va || b.sequence>a.sequence) ? 1 : 0;
        savedCounts=countSlot==0 ? a : b;
        if (savedCounts.date/10000!=today/10000 || savedCounts.date>today) {
            reportCountFault("RESTORE_FAILED_DATE"); return false;
        }
        if (savedCounts.date<today) {
            closedCounts=savedCounts;
            if (!flash && !persistClosedCounts()) return false;
        }
        totalDailyCars=savedCounts.date==today ? savedCounts.daily : 0;
        totalShowCars=savedCounts.show;
        for(int i=0;i<24;++i) hourlyCount[i]=savedCounts.date==today ? savedCounts.hourly[i] : 0;
        for(int i=0;i<24;++i) officialHourly[i]=savedCounts.date==today ? savedCounts.official[i]:0;
        officialCountsComplete=savedCounts.date!=today || savedCounts.officialComplete;
        countDate=today;
        showResetDate=savedCounts.date==today ? savedCounts.showResetDate : 0;
    } else {
        // Existing damaged checkpoints must not silently fall back to stale CSVs.
        if (SD.exists(countPath("CountState.a")) || SD.exists(countPath("CountState.b"))) {
            reportCountFault("RESTORE_FAILED_KEEP_FILES"); return false;
        }
        if (freshCountYear==int(today/10000)) {
            totalDailyCars=totalShowCars=0;
            for(int i=0;i<24;++i) hourlyCount[i]=0;
            countDate=today;
        } else if (!loadLegacyCounts(today)) {
            reportCountFault("RESTORE_FAILED_KEEP_FILES"); return false;
        }
    }
    // Before today's show begins, a new official series is known complete.
    // Mid-show migration remains explicitly incomplete; never fabricate history.
    if (officialSeriesCanStart()) officialCountsComplete=true;
    countReady=true;
    dayOfMonth=lastDayOfMonth=today%100;
    daysRunning=recordingDaysRunning();
    flagDailyShowStartReset=showResetDate==today;
    if (persistCountState()) {
        // Inspect the resulting copies, not pre-migration validation flags.
        traffic::CountRecord afterA{},afterB{};
        if (readCountRecord(0,afterA) && readCountRecord(1,afterB))
            reportCountFault("OK");
        else reportCountFault("RECOVERED_ONE_VALID_COPY_CHECK_SD");
    }
    return true;
}

bool checkpointCountState(bool exportSD) {
    if (!countReady) return false;
    if (!countToday() || countToday()!=countDate) {
        reportCountFault("SAVE_FAILED_CLOCK_OR_SD"); return false;
    }
    traffic::CountRecord next{};
    const traffic::CountRecord* flash=latestTrafficSnapshot();
    uint32_t sequence=flash && flash->sequence>savedCounts.sequence?flash->sequence:savedCounts.sequence;
    if(sequence==UINT32_MAX){reportCountFault("SAVE_FAILED_SEQUENCE_FULL");return false;}
    next.magic=0x4d534232UL; next.sequence=sequence+1;
    next.date=countDate; next.daily=totalDailyCars; next.show=totalShowCars;
    next.showResetDate=showResetDate;
    for(int i=0;i<24;++i) next.hourly[i]=hourlyCount[i];
    for(int i=0;i<24;++i) next.official[i]=officialHourly[i];
    next.officialComplete=officialCountsComplete;
    next.checksum=traffic::countChecksum(next);
    if (!traffic::validCountRecord(next)) { reportCountFault("SAVE_FAILED_INVALID_COUNTS"); return false; }
    bool flashSame=flash && memcmp(&next.date,&flash->date,offsetof(traffic::CountRecord,checksum)-offsetof(traffic::CountRecord,date))==0;
    bool flashOk=flashSame;
    if(flashSame)next=*flash;
    else flashOk=appendTrafficSnapshot(next);
    if(!exportSD)return flashOk;
    // SD loss does not prevent the flash commit above. Exports remain retryable.
    if(!sdAvailable || SD.cardType()==CARD_NONE || !persistClosedCounts()) {
        reportCountFault(flashOk?"OK_FLASH_SD_PENDING":"SAVE_FAILED_FLASH_AND_SD");return false;
    }
    // Skip unchanged state; retry a failed changed state on the main-loop timer.
    if (sdCheckpointKnown && savedCounts.sequence && savedCounts.officialComplete==next.officialComplete && memcmp(&next.date,&savedCounts.date,offsetof(traffic::CountRecord,checksum)-offsetof(traffic::CountRecord,date))==0) {reportCountFault(flashOk?"OK":"SD_ONLY_FLASH_FAULT");return true;}
    int target=countSlot==0 ? 1 : 0;
    File f=recordingOpen(countPath(target==0 ? "CountState.a" : "CountState.b"),FILE_WRITE);
    if (!f) { reportCountFault(flashOk?"OK_FLASH_SD_PENDING":"SAVE_FAILED_OPEN"); return false; }
    size_t n=f.write(reinterpret_cast<const uint8_t*>(&next),sizeof(next));
    f.flush(); f.close();
    traffic::CountRecord check{};
    if (n!=sizeof(next) || !readCountRecord(target,check) || memcmp(&next,&check,sizeof(next))!=0) {
        reportCountFault(flashOk?"OK_FLASH_SD_PENDING":"SAVE_FAILED_VERIFY"); return false;
    }
    savedCounts=next; countSlot=target;sdCheckpointKnown=true;
    reportCountFault(flashOk?"OK":"SD_ONLY_FLASH_FAULT");
    return true;
}

bool persistCountState(){return checkpointCountState(true);}
bool commitCountJournal(){return checkpointCountState(false);}

void serviceCountState() {
    if (!countReady) {
        if (!retryClockRestore || !countToday()) return;
        if (millis()-lastRestoreAttempt<1000UL) return;
        lastRestoreAttempt=millis();
        if (restoreCountState()) {
            // Repair exports only after the verified snapshot is loaded.
            saveDailyTotal(); saveShowTotal(); saveHourlyCounts();
            publishCountHealth();
        }
        return;
    }
    uint32_t today=countToday();
    if (!today || today<countDate) { reportCountFault("CLOCK_INVALID_OR_BACKWARD"); return; }
    if (today>countDate) {
        if (today/10000==countDate/10000) {
            // Archive the exact closed day before either alternating checkpoint
            // can be replaced. Failed archive blocks new checkpoints, not sensing.
            closedCounts=savedCounts;
            closedCounts.date=countDate;closedCounts.daily=totalDailyCars;closedCounts.show=totalShowCars;
            closedCounts.showResetDate=showResetDate;closedCounts.officialComplete=officialCountsComplete;
            for(int i=0;i<24;++i) {closedCounts.hourly[i]=hourlyCount[i];closedCounts.official[i]=officialHourly[i];}
            closedCounts.checksum=traffic::countChecksum(closedCounts);
        }
        // Full-date rollover, including a missed midnight/reboot. Never reset
        // at 23:59 or rely on an exact second to clear daily flags.
        if (today/10000!=countDate/10000) { totalShowCars=0; countSlot=-1; }
        countDate=today; showResetDate=0;
        totalDailyCars=0;
        officialCountsComplete=true;
        for (int i=0;i<24;++i) { hourlyCount[i]=0;officialHourly[i]=0; }
        flagDailyShowStartReset=false;
        flagDailyShowSummarySaved=false;
        flagDailySummarySaved=false;
        flagDaysRunningReset=false;
        daysRunning=recordingDaysRunning();
        dayOfMonth=lastDayOfMonth=today%100;
        persistCountState();
        saveDailyTotal(); saveShowTotal(); saveDayOfMonth();
    }
    static unsigned long lastRetry=0;
    if (millis()-lastRetry>=5000UL) { lastRetry=millis(); persistCountState(); }
}

bool resetCountsForShow() {
    detectCar(); // Apply queued pre-reset passages before writing the reset snapshot.
    uint32_t today=countToday();
    if (!countReady || !today || countDate!=today || showResetDate==today) return false;
    // Persist the reset marker together with the reset total. A reboot within
    // the same reset minute must not erase vehicles counted after that reset.
    totalDailyCars=0; showResetDate=today;
    officialCountsComplete=true;
    for (int i=0;i<24;++i) officialHourly[i]=0;
    flagDailyShowStartReset=true;
    persistCountState();
    saveDailyTotal();
    return true;
}

// A correction's identity and resulting count share ONE flash commit. NVS
// acknowledgement may follow; losing power cannot consume an unapplied command
// or replay a correction over vehicles counted after it.
bool correctCountState(bool daily,uint32_t value,uint32_t nonce) {
    if(!countEventCanApply() || value>INT_MAX || nonce<=journalCorrectionNonce())return false;
    const traffic::CountRecord* flash=latestTrafficSnapshot();
    uint32_t sequence=flash && flash->sequence>savedCounts.sequence?flash->sequence:savedCounts.sequence;
    if(sequence==UINT32_MAX)return false;
    traffic::CountRecord next{};next.magic=0x4d534232;next.sequence=sequence+1;
    next.date=countDate;next.daily=daily?value:totalDailyCars;next.show=daily?totalShowCars:value;
    next.showResetDate=showResetDate;next.officialComplete=officialCountsComplete;
    for(int i=0;i<24;++i){next.hourly[i]=hourlyCount[i];next.official[i]=officialHourly[i];}
    next.checksum=traffic::countChecksum(next);
    if(!traffic::validCountRecord(next) || !appendTrafficCorrection(next,nonce)) {
        reportCountFault("CORRECTION_NOT_APPLIED_FLASH_FAULT");return false;
    }
    totalDailyCars=next.daily;totalShowCars=next.show;persistCountState();return true;
}

// Keep the previous complete hourly CSV until its replacement is verified.
// The checkpoint protects current counts; this protects older CSV rows too.
bool recoverHourlyExport(const String& path) {
    if (SD.exists(path)) return true;
    String previous=path+".previous";
    if (!SD.exists(previous)) return true; // Normal first-time initialization.
    if (recordingRename(previous,path)) return true;
    reportHourlyExportStatus("RECOVERY_FAILED");
    return false;
}

bool writeHourlyExport(const String& path, const String& content) {
    String pending=path+".pending", previous=path+".previous";
    File f=recordingOpen(pending,FILE_WRITE);
    size_t n=f ? f.print(content) : 0;
    f.flush(); f.close();
    File check=recordingOpen(pending,FILE_READ);
    bool ok=check && check.size()==content.length() && n==content.length() && check.readString()==content;
    check.close();
    if (!ok) {
        reportHourlyExportStatus("WRITE_VERIFY_FAILED");
        return false;
    }
    if (!recoverHourlyExport(path) ||
        (SD.exists(previous) && !recordingRemove(previous)) ||
        (SD.exists(path) && !recordingRename(path,previous))) {
        reportHourlyExportStatus("BACKUP_FAILED");
        return false;
    }
    if (!recordingRename(pending,path)) {
        // Best effort rollback. On power loss/failed rollback, boot restores
        // .previous BEFORE the initializer can create an empty hourly file.
        recoverHourlyExport(path);
        reportHourlyExportStatus("REPLACE_FAILED");
        return false;
    }
    reportHourlyExportStatus("OK");
    return true;
}

// ClosedCounts is a one-time closed-day archive, never a restore input. Permit
// explicit housekeeping only after a current-day SD checkpoint is verified.
// No wildcard, nested path, journal record or active checkpoint can enter here.
bool removeObsoleteClosedCount(const String& path) {
    if(!countReady || !sdAvailable || !sdCheckpointKnown || strcmp(countFault,"OK") ||
       !countDate || countToday()!=countDate || savedCounts.date!=countDate) return false;
    const String prefix=String(seasonFolder)+"/ClosedCounts-";
    if(!path.startsWith(prefix) || path.length()!=prefix.length()+12 || !path.endsWith(".bin")) return false;
    uint32_t date=0;
    for(unsigned i=0;i<8;++i){char c=path[prefix.length()+i];if(c<'0'||c>'9')return false;date=date*10+c-'0';}
    if(!traffic::countDateValid(date) || date/10000!=countDate/10000 || date>=countDate || closedCounts.date==date) return false;
    traffic::CountRecord checkpoint{},old{};
    bool newer=false;
    for(int slot=0;slot<2;++slot) if(readCountRecord(slot,checkpoint) && checkpoint.date==countDate &&
        checkpoint.sequence>=savedCounts.sequence) newer=true;
    if(!newer) return false;
    File f=recordingOpen(path,FILE_READ);
    bool valid=f && f.size()==sizeof(old) && f.read(reinterpret_cast<uint8_t*>(&old),sizeof(old))==sizeof(old);
    f.close();
    if(!valid || !traffic::validCountRecord(old) || old.date!=date || old.sequence>=savedCounts.sequence) return false;
    // Recheck rollover/pending state after I/O. A failed delete leaves data intact.
    if(countToday()!=countDate || closedCounts.date==date || !sdAvailable) return false;
    return recordingRemove(path);
}
