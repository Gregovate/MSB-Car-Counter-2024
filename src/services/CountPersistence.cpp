#include "traffic/Firmware.h"
#include "traffic/CountRecord.h"
#include <limits.h>

// Identical in both devices. Legacy text/CSV files remain for the web UI;
// CountState.a/b are the authoritative, verified reboot checkpoint from .4 on.
namespace {
traffic::CountRecord savedCounts{};
uint32_t countDate=0, showResetDate=0;
int freshCountYear=0;
int countSlot=-1;
bool countReady=false;
const char* countFault="NOT_RESTORED";

uint32_t countToday() {
    if (!rtcReady || rtc.lostPower()) return 0;
    DateTime now=rtc.now();
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
    bool ok=f.size()==sizeof(record) && f.read(reinterpret_cast<uint8_t*>(&record),sizeof(record))==sizeof(record);
    f.close();
    return ok && traffic::validCountRecord(record);
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

void publishCountHealth() {
    reportCountFault(countFault);
    if (!countReady)
        publishMQTT((String(topic_base_path)+"/System/unrestoredDailyCount").c_str(),String(totalDailyCars),true);
}

bool countsReady() { return countReady; }

bool countTopicPublishable(const char* topic) {
    if (countReady) return true;
    String t(topic), base(topic_base_path);
    // Never replace retained counts/heartbeat with startup defaults after a
    // recovery failure. Health/config/alarms continue to publish normally.
    return !t.startsWith(base+"/Cars/") && t!=base+"/System/heartbeat";
}

bool restoreCountState() {
    uint32_t today=countToday();
    if (!today || SD.cardType()==CARD_NONE) { reportCountFault("RESTORE_FAILED_CLOCK_OR_SD"); return false; }
    traffic::CountRecord a{},b{};
    bool va=readCountRecord(0,a), vb=readCountRecord(1,b);
    if (va || vb) {
        countSlot=vb && (!va || b.sequence>a.sequence) ? 1 : 0;
        savedCounts=countSlot==0 ? a : b;
        if (savedCounts.date/10000!=today/10000 || savedCounts.date>today) {
            reportCountFault("RESTORE_FAILED_DATE"); return false;
        }
        totalDailyCars=savedCounts.date==today ? savedCounts.daily : 0;
        totalShowCars=savedCounts.show;
        for(int i=0;i<24;++i) hourlyCount[i]=savedCounts.date==today ? savedCounts.hourly[i] : 0;
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
    countReady=true;
    dayOfMonth=lastDayOfMonth=today%100;
    daysRunning=recordingDaysRunning();
    flagDailyShowStartReset=showResetDate==today;
    persistCountState();
    if (va!=vb && (SD.exists(countPath("CountState.a")) && SD.exists(countPath("CountState.b"))))
        reportCountFault("RECOVERED_ONE_VALID_COPY_CHECK_SD");
    return true;
}

bool persistCountState() {
    if (!countReady) return false;
    if (SD.cardType()==CARD_NONE || !countToday() || countToday()!=countDate) {
        reportCountFault("SAVE_FAILED_CLOCK_OR_SD"); return false;
    }
    traffic::CountRecord next{};
    next.magic=0x4d534231UL; next.sequence=savedCounts.sequence+1;
    next.date=countDate; next.daily=totalDailyCars; next.show=totalShowCars;
    next.showResetDate=showResetDate;
    for(int i=0;i<24;++i) next.hourly[i]=hourlyCount[i];
    next.checksum=traffic::countChecksum(next);
    if (!traffic::validCountRecord(next)) { reportCountFault("SAVE_FAILED_INVALID_COUNTS"); return false; }
    // Skip unchanged state; retry a failed changed state on the main-loop timer.
    if (savedCounts.sequence && memcmp(&next.date,&savedCounts.date,offsetof(traffic::CountRecord,checksum)-offsetof(traffic::CountRecord,date))==0) return true;
    int target=countSlot==0 ? 1 : 0;
    File f=recordingOpen(countPath(target==0 ? "CountState.a" : "CountState.b"),FILE_WRITE);
    if (!f) { reportCountFault("SAVE_FAILED_OPEN"); return false; }
    size_t n=f.write(reinterpret_cast<const uint8_t*>(&next),sizeof(next));
    f.flush(); f.close();
    traffic::CountRecord check{};
    if (n!=sizeof(next) || !readCountRecord(target,check) || memcmp(&next,&check,sizeof(next))!=0) {
        reportCountFault("SAVE_FAILED_VERIFY"); return false;
    }
    savedCounts=next; countSlot=target;
    reportCountFault("OK");
    return true;
}

void serviceCountState() {
    if (!countReady) return; // Recovery needs operator inspection; don't erase evidence.
    uint32_t today=countToday();
    if (!today || today<countDate) { reportCountFault("CLOCK_INVALID_OR_BACKWARD"); return; }
    if (today>countDate) {
        // Full-date rollover, including a missed midnight/reboot. Never reset
        // at 23:59 or rely on an exact second to clear daily flags.
        if (today/10000!=countDate/10000) { totalShowCars=0; savedCounts={}; countSlot=-1; }
        countDate=today; showResetDate=0;
        totalDailyCars=0;
        for (int i=0;i<24;++i) hourlyCount[i]=0;
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
    uint32_t today=countToday();
    if (!countReady || !today || countDate!=today || showResetDate==today) return false;
    // Persist the reset marker together with the reset total. A reboot within
    // the same reset minute must not erase vehicles counted after that reset.
    totalDailyCars=0; showResetDate=today;
    flagDailyShowStartReset=true;
    persistCountState();
    saveDailyTotal();
    return true;
}

// Keep the previous complete hourly CSV until its replacement is verified.
// The checkpoint protects current counts; this protects older CSV rows too.
bool recoverHourlyExport(const String& path) {
    if (SD.exists(path)) return true;
    String previous=path+".previous";
    if (!SD.exists(previous)) return true; // Normal first-time initialization.
    if (recordingRename(previous,path)) return true;
    publishMQTT((String(topic_base_path)+"/System/hourlyExport").c_str(),"RECOVERY_FAILED",true);
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
        publishMQTT((String(topic_base_path)+"/System/hourlyExport").c_str(),"WRITE_VERIFY_FAILED",true);
        return false;
    }
    if (!recoverHourlyExport(path) ||
        (SD.exists(previous) && !recordingRemove(previous)) ||
        (SD.exists(path) && !recordingRename(path,previous))) {
        publishMQTT((String(topic_base_path)+"/System/hourlyExport").c_str(),"BACKUP_FAILED",true);
        return false;
    }
    if (!recordingRename(pending,path)) {
        // Best effort rollback. On power loss/failed rollback, boot restores
        // .previous BEFORE the initializer can create an empty hourly file.
        recoverHourlyExport(path);
        publishMQTT((String(topic_base_path)+"/System/hourlyExport").c_str(),"REPLACE_FAILED",true);
        return false;
    }
    publishMQTT((String(topic_base_path)+"/System/hourlyExport").c_str(),"OK",true);
    return true;
}
