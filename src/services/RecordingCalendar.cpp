#include "traffic/Firmware.h"
#include <Preferences.h>

// Shared implementation. Persistent high-water year is independent of HA dates.
namespace {
Preferences calendarPrefs;
int activeYear = traffic::kRecordingYearFloor;
int networkYear = 0;
uint32_t confirmedStart=0;
uint32_t daysAnchor=0;
void daysAnchorKey(char (&key)[16]) {
    snprintf(key,sizeof(key),"days%08lu",(unsigned long)confirmedStart);
}
int startMonth = 11;
int startDay = traffic::blackFridayDay(traffic::kRecordingYearFloor);
bool initialized = false;
bool rtcTimeLossObserved = false;
bool nvsDateValid = false;
bool sdCalendarReady = false;
const char* calendarFault = "OK";

uint32_t calendarChecksum(int y, int m, int d) {
    return 0x4d534243UL ^ (uint32_t(y) << 16) ^ (uint32_t(m) << 8) ^ uint32_t(d);
}
void persistCalendar() {
    // NVS protects the high-water year if the card is missing/replaced. SD keeps
    // two independent records; a torn write never requires deleting the old one.
    if (calendarPrefs.getInt("year", 0) != activeYear &&
        calendarPrefs.putInt("year", activeYear) != sizeof(int32_t))
        calendarFault = "YEAR_NVS_WRITE_FAILED";
    uint32_t date = activeYear * 10000UL + startMonth * 100UL + startDay;
    if (calendarPrefs.getUInt("start", 0) != date &&
        calendarPrefs.putUInt("start", date) != sizeof(uint32_t))
        calendarFault = "DATE_NVS_WRITE_FAILED";
    if (calendarPrefs.getInt("year",0) != activeYear)
        calendarFault = "YEAR_NVS_VERIFY_FAILED";
    if (calendarPrefs.getUInt("start",0) != date)
        calendarFault = "DATE_NVS_VERIFY_FAILED";
    if (!sdCalendarReady) return;
    char record[64];
    snprintf(record, sizeof(record), "%d %d %d %lu\n", activeYear, startMonth,
             startDay, (unsigned long)calendarChecksum(activeYear,startMonth,startDay));
    for (const char* path : {"/recording-calendar.a", "/recording-calendar.b"}) {
        File file = SD.open(path, FILE_WRITE);
        if (!file) { calendarFault = "CALENDAR_SD_WRITE_FAILED"; return; }
        size_t written = file.print(record);
        file.flush(); file.close();
        File check = SD.open(path, FILE_READ);
        String actual = check ? check.readStringUntil('\n') : String("");
        check.close();
        String expected(record); expected.trim();
        if (written != strlen(record) || actual != expected) {
            calendarFault = "CALENDAR_SD_VERIFY_FAILED";
            return; // Keep the other copy intact.
        }
    }
}
bool usableRtc() {
    if (!rtcReady || rtc.lostPower()) return false;
    DateTime now = rtc.now();
    return now.isValid() && traffic::validYear(now.year());
}
}

int recordingYear() { return activeYear; }

void initializeRecordingCalendar() {
    if (initialized) return;
    initialized = true;
    // Capture OSF before NTP adjustment clears it. This is time-loss evidence,
    // not a battery voltage/charge measurement.
    rtcTimeLossObserved=rtcReady && rtc.lostPower();
    if (!calendarPrefs.begin("traffic-cal", false)) calendarFault = "CALENDAR_NVS_UNAVAILABLE";
    confirmedStart=calendarPrefs.getUInt("ha-start",0);
    char key[16];daysAnchorKey(key);daysAnchor=calendarPrefs.getUInt(key,0);
    int saved = calendarPrefs.getInt("year", traffic::kRecordingYearFloor);
    activeYear = traffic::validYear(saved) ? saved : traffic::kRecordingYearFloor;
    int clockYear = usableRtc() ? rtc.now().year() : 0;
    activeYear = traffic::selectRecordingYear(activeYear, clockYear, networkYear == clockYear);
    startMonth = 11; startDay = traffic::blackFridayDay(activeYear);
    uint32_t date = calendarPrefs.getUInt("start", 0);
    int y = date / 10000, m = date / 100 % 100, d = date % 100;
    if (y == activeYear && traffic::validDate(y,m,d)) {
        startMonth=m; startDay=d; nvsDateValid=true;
    }
    if(confirmedStart/10000==static_cast<uint32_t>(activeYear) &&
       traffic::validDate(activeYear,confirmedStart/100%100,confirmedStart%100)) {
        startMonth=confirmedStart/100%100; startDay=confirmedStart%100;
    }
    persistCalendar();
    applyRecordingCalendar();
}

void loadRecordingCalendarFromSD() {
    initializeRecordingCalendar();
    bool loadedDate=nvsDateValid;
    // Select only verified records. Never adopt a lower recording year.
    for (const char* path : {"/recording-calendar.a", "/recording-calendar.b"}) {
        File file = SD.open(path, FILE_READ);
        if (!file) continue;
        String line = file.readStringUntil('\n'); file.close();
        int y=0,m=0,d=0; unsigned long checksum=0; char tail=0;
        if (sscanf(line.c_str(), "%d %d %d %lu %c", &y,&m,&d,&checksum,&tail) != 4 ||
            !traffic::validDate(y,m,d) || checksum != calendarChecksum(y,m,d)) continue;
        if (y > activeYear) {
            activeYear=y; startMonth=m; startDay=d; loadedDate=true;
        } else if (y == activeYear && !loadedDate) {
            startMonth=m; startDay=d; loadedDate=true;
        }
    }
    sdCalendarReady = true;
    if(confirmedStart/10000==static_cast<uint32_t>(activeYear) &&
       traffic::validDate(activeYear,confirmedStart/100%100,confirmedStart%100)) {
        startMonth=confirmedStart/100%100; startDay=confirmedStart%100;
    }
    persistCalendar();
    applyRecordingCalendar();
    publishRecordingCalendar();
}

DateTime recordingShowStart() { return DateTime(activeYear,startMonth,startDay,0,0,0); }

bool acceptShowStartDate(const String &date) {
    int y=0,m=0,d=0; char tail=0;
    if (date.length()!=10 || sscanf(date.c_str(), "%d-%d-%d%c", &y,&m,&d,&tail)!=3 ||
        y!=activeYear || !traffic::validDate(y,m,d)) {
        publishMQTT(MQTT_DEBUG_LOG, "Rejected stale/invalid show start date: " + date);
        return false;
    }
    const uint32_t requested=y*10000UL+m*100UL+d;
    if(productionRequested() && requested!=productionStartDate())return false;
    // An explicitly received HA calendar is distinct from the computed fallback.
    // Save it even if the local fallback already happens to have the same date.
    if(confirmedStart!=requested) {
        if(calendarPrefs.putUInt("ha-start",requested)!=sizeof(requested) ||
           calendarPrefs.getUInt("ha-start",0)!=requested){calendarFault="HA_DATE_SAVE_FAILED";return false;}
        confirmedStart=requested;char key[16];daysAnchorKey(key);daysAnchor=calendarPrefs.getUInt(key,0);
    }
    if (m!=startMonth || d!=startDay) {
        startMonth=m; startDay=d;
        persistCalendar();
        applyRecordingCalendar();
    }
    nvsDateValid=true;
    daysRunning=recordingDaysRunning();
    publishRecordingCalendar();
    return true;
}

bool productionDateMatches() {
    if(!confirmedStart || !usableRtc())return false;
    DateTime now=rtc.now();
    return confirmedStart==now.year()*10000UL+now.month()*100UL+now.day();
}

int scheduledDaysRunning() {
    if (!usableRtc()) return 0;
    DateTime now=rtc.now(), start=recordingShowStart();
    if (now.year()!=activeYear || now.unixtime()<start.unixtime()) return 0;
    int end=operatingSeasonEndDate(),down=operatingDownDate();
    DateTime endDay(end/10000,end/100%100,end%100);
    DateTime downDay(down/10000,down/100%100,down%100);
    uint32_t last=now.unixtime()<endDay.unixtime() ? now.unixtime():endDay.unixtime();
    if (last<start.unixtime()) return 0;
    int days=(last-start.unixtime())/86400UL+1;
    if (downDay.unixtime()>=start.unixtime() && downDay.unixtime()<=last) --days;
    return days;
}

int recordingDaysRunning() {
    int scheduled=scheduledDaysRunning();
    if(!daysAnchor || confirmedStart/10000!=static_cast<uint32_t>(activeYear))return scheduled;
    int anchorScheduled=daysAnchor>>16,actual=daysAnchor&0xffff;
    if(anchorScheduled<1 || anchorScheduled>366 || actual>anchorScheduled || scheduled<anchorScheduled)return scheduled;
    return actual+scheduled-anchorScheduled;
}

bool acceptDaysRunning(const char* payload) {
    if(!productionActive() || !confirmedStart || !usableRtc() || !payload || !*payload)return false;
    unsigned value=0;
    for(const char* p=payload;*p;++p) {
        if(*p<'0' || *p>'9' || value>36)return false;
        value=value*10+(*p-'0');
    }
    int scheduled=scheduledDaysRunning();
    if(scheduled<1 || value>static_cast<unsigned>(scheduled))return false;
    uint32_t anchor=(static_cast<uint32_t>(scheduled)<<16)|value;
    return applyDaysAnchor(confirmedStart,anchor);
}

bool applyDaysAnchor(uint32_t season,uint32_t anchor) {
    const uint32_t scheduled=anchor>>16,actual=anchor&0xffff;
    if(!productionActive() || season!=confirmedStart || !scheduled || actual>scheduled ||
       scheduled>static_cast<uint32_t>(scheduledDaysRunning()) || scheduled<(daysAnchor>>16))return false;
    if(anchor==daysAnchor)return false;
    char key[16];daysAnchorKey(key);
    if(calendarPrefs.putUInt(key,anchor)!=sizeof(anchor) ||
       calendarPrefs.getUInt(key,0)!=anchor){calendarFault="DAYS_RUNNING_SAVE_FAILED";return false;}
    daysAnchor=anchor;daysRunning=recordingDaysRunning();publishRecordingCalendar();return true;
}

bool recordingShowSeasonActive() {
    if (!usableRtc()) return false;
    DateTime now=rtc.now();
    int date=now.year()*10000+now.month()*100+now.day();
    return now.year()==activeYear && now.unixtime()>=recordingShowStart().unixtime() &&
           date<=operatingSeasonEndDate() && date!=operatingDownDate();
}

void noteNetworkClock(int year) { networkYear=year; }

void updateRecordingCalendar() {
    static unsigned long checked=0;
    if (millis()-checked<1000UL) return;
    checked=millis();
    static unsigned long lastNtpAttempt=0;
    if (millis()-lastNtpAttempt >= 60000UL) {
        lastNtpAttempt=millis();
        SetLocalTime();
    }
    if (rtcReady && rtc.lostPower()) rtcTimeLossObserved=true;
    int clockYear=usableRtc() ? rtc.now().year() : 0;
    int next=traffic::selectRecordingYear(activeYear,clockYear,networkYear==clockYear);
    if (next>activeYear) {
        activeYear=next;
        startMonth=11; startDay=traffic::blackFridayDay(activeYear);
        persistCalendar();
        applyRecordingCalendar();
        // Path rotation is not a count reset. Existing daily scheduling remains
        // responsible for daily totals; no archive is read or rewritten here.
        publishRecordingCalendar();
    }
    if (clockYear!=activeYear) calendarFault="CLOCK_YEAR_MISMATCH_USING_SAVED_YEAR";
    if (clockYear==activeYear && strcmp(calendarFault,"CLOCK_YEAR_MISMATCH_USING_SAVED_YEAR")==0) calendarFault="OK";
    daysRunning=recordingDaysRunning();
}

void publishRecordingCalendar() {
    String base=String(topic_base_path);
    if (rtcReady && rtc.lostPower()) rtcTimeLossObserved=true;
    const char* rtcHealth=!rtcReady ? "NOT_DETECTED" :
        !usableRtc() ? "TIME_INVALID" :
        rtcTimeLossObserved ? "TIME_RECOVERED_CHECK_RTC_BACKUP" : "OK";
    publishMQTT((base+"/System/rtcHealth").c_str(),rtcHealth,true);
    publishMQTT((base+"/System/rtcTimeLossObserved").c_str(),rtcTimeLossObserved ? "1":"0",true);
    publishMQTT(MQTT_PUB_SEASON_YEAR,String(activeYear),true);
    publishMQTT(MQTT_PUB_SEASON_FOLDER,String(seasonFolder),true);
    DateTime start=recordingShowStart();
    /* Allow full RTC component ranges; format widths are minimums. */ char date[24]; snprintf(date,sizeof(date),"%04d-%02d-%02d",start.year(),start.month(),start.day());
    if(daysAnchor && productionActive()) {
        char adjusted[48];
        snprintf(adjusted,sizeof(adjusted),"%lu,%lu",(unsigned long)confirmedStart,(unsigned long)daysAnchor);
        publishMQTT((base+"/Calendar/daysRunningAnchor").c_str(),adjusted,true);
    }
    // Report on State, never back onto the HA command topic.
    publishMQTT((base+"/State/showStartDate").c_str(),date,true);
    publishMQTT((base+"/System/calendarHealth").c_str(),calendarFault,true);
    publishMQTT(MQTT_PUB_DAYSRUNNING,String(recordingDaysRunning()),true);
}

bool recordingPathWritable(const String &path, bool mkdirOnly) {
    // A write arriving at the year boundary must not use the old open path even
    // before the main loop has rotated the filename variables.
    if (productionBlocked() || productionCardRejected()) return false;
    int guardYear=activeYear;
    if (usableRtc()) guardYear=traffic::selectRecordingYear(activeYear,rtc.now().year(),networkYear==rtc.now().year());
    return traffic::writablePath(path.c_str(),guardYear,mkdirOnly);
}

File recordingOpen(const String &path, const char* mode) {
    if (productionBlocked() || productionCardRejected()) return File();
    if (strcmp(mode,FILE_READ)!=0 && !recordingPathWritable(path)) {
        Serial.printf("Blocked protected SD write: %s\n",path.c_str());
        return File();
    }
    return SD.open(path,mode);
}
bool recordingMkdir(const String &path) {
    return recordingPathWritable(path,true) && SD.mkdir(path);
}
bool recordingRemove(const String &path) {
    return recordingPathWritable(path) && SD.remove(path);
}
bool recordingRename(const String &from, const String &to) {
    return recordingPathWritable(from) && recordingPathWritable(to) && SD.rename(from,to);
}

// User file-manager operations must not edit the internal recovery records.
bool recordingWebWritable(const String &path) {
    String protectedPath(path);protectedPath.toLowerCase();
    if (protectedPath.indexOf("operating-settings")>=0 || protectedPath.endsWith(".pending") || protectedPath.endsWith(".previous")) return false;

    String lower=path; lower.toLowerCase();
    return !lower.startsWith("/recording-calendar.") && lower.indexOf("/countstate.")<0 && lower.indexOf("/closedcounts-")<0 &&
           lower.indexOf("hourlydata.csv.")<0 && lower.indexOf("/trafficjournal")<0 && recordingPathWritable(path);
}

namespace {
bool transitionFolderAllowed(const String& active) {
    return productionRequested() && productionBlocked() &&
        (active==String("/CC/")+String(recordingYear()) || active==String("/GC/")+String(recordingYear()));
}
}
bool archiveProductionTestFolder(const String& active) {
    return transitionFolderAllowed(active) && SD.rename(active,active+"-TEST");
}
bool makeProductionFolder(const String& active) {
    return transitionFolderAllowed(active) && SD.mkdir(active);
}
File openProductionBaseline(const String& active,const char* mode) {
    if(!transitionFolderAllowed(active))return File();
    return SD.open(active+"/CountState.a",mode);
}

File readProductionCardMarker(const char* path) {
    // Mount validation must be able to read the marker on a newly replaced card.
    return SD.open(path,FILE_READ);
}
