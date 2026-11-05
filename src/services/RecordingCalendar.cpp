#include "traffic/Firmware.h"
#include <Preferences.h>

// Shared implementation. Persistent high-water year is independent of HA dates.
namespace {
Preferences calendarPrefs;
int activeYear = traffic::kRecordingYearFloor;
int networkYear = 0;
int startMonth = 11;
int startDay = traffic::blackFridayDay(traffic::kRecordingYearFloor);
bool initialized = false;
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
    if (!calendarPrefs.begin("traffic-cal", false)) calendarFault = "CALENDAR_NVS_UNAVAILABLE";
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

int recordingDaysRunning() {
    if (!usableRtc()) return 0;
    DateTime now=rtc.now(), start=recordingShowStart();
    if (now.year()!=activeYear || now.unixtime()<start.unixtime()) return 0;
    int days=(now.unixtime()-start.unixtime())/86400UL+1;
    if ((startMonth<12 || (startMonth==12 && startDay<24)) &&
        now.month()==12 && now.day()>=24) --days;
    return days;
}

bool recordingShowSeasonActive() {
    if (!usableRtc()) return false;
    DateTime now=rtc.now();
    return now.year()==activeYear && now.unixtime()>=recordingShowStart().unixtime() &&
           !(now.month()==12 && now.day()==24);
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
    daysRunning=recordingDaysRunning();
}

void publishRecordingCalendar() {
    String base=String(topic_base_path);
    publishMQTT(MQTT_PUB_SEASON_YEAR,String(activeYear),true);
    publishMQTT(MQTT_PUB_SEASON_FOLDER,String(seasonFolder),true);
    DateTime start=recordingShowStart();
    char date[11]; snprintf(date,sizeof(date),"%04d-%02d-%02d",start.year(),start.month(),start.day());
    // Report on State, never back onto the HA command topic.
    publishMQTT((base+"/State/showStartDate").c_str(),date,true);
    publishMQTT((base+"/System/calendarHealth").c_str(),calendarFault,true);
    publishMQTT(MQTT_PUB_DAYSRUNNING,String(recordingDaysRunning()),true);
}

bool recordingPathWritable(const String &path, bool mkdirOnly) {
    // A write arriving at the year boundary must not use the old open path even
    // before the main loop has rotated the filename variables.
    int guardYear=activeYear;
    if (usableRtc()) guardYear=traffic::selectRecordingYear(activeYear,rtc.now().year(),networkYear==rtc.now().year());
    return traffic::writablePath(path.c_str(),guardYear,mkdirOnly);
}

File recordingOpen(const String &path, const char* mode) {
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
    String lower=path; lower.toLowerCase();
    return !lower.startsWith("/recording-calendar.") && recordingPathWritable(path);
}
