#include "traffic/Firmware.h"

namespace {
const char* summaryHealth="NOT_DUE";
// New columns are appended, preserving all existing column positions and rows.
// Prior-year files never pass recordingOpen's write guard.
String summaryPath(const char* name) { return String(seasonFolder)+"/"+name; }
bool summaryReplace(const String& path,const String& text) {
    String pending=path+".pending",previous=path+".previous";
    File f=recordingOpen(pending,FILE_WRITE);
    size_t n=f ? f.print(text):0;f.flush();f.close();
    File check=recordingOpen(pending,FILE_READ);
    bool ok=check && n==text.length() && check.size()==text.length() && check.readString()==text;
    check.close();
    if (!ok) return false;
    if (!SD.exists(path) && SD.exists(previous) && !recordingRename(previous,path)) return false;
    if (SD.exists(previous) && !recordingRemove(previous)) return false;
    if (SD.exists(path) && !recordingRename(path,previous)) return false;
    if (recordingRename(pending,path)) return true;
    if (SD.exists(previous)) recordingRename(previous,path);
    return false;
}
bool upsertSummary(const String& path,const String& header,const String& date,const String& row) {
    // Recovery precedes any initialization. The prior complete CSV survives
    // power loss during replacement; retries update a date rather than duplicate it.
    if (!SD.exists(path) && SD.exists(path+".previous") && !recordingRename(path+".previous",path)) return false;
    String content=header+",10PM,ExtensionMin\n";
    File f=recordingOpen(path,FILE_READ);
    if (!f && SD.exists(path)) return false;
    if (f) {
        // Bound memory use; refuse to truncate an unexpectedly large metrics file.
        if (f.size()>65536) { f.close();return false; }
        String oldHeader=f.readStringUntil('\n');oldHeader.trim();
        bool oldSchema=oldHeader==header;
        if (!oldSchema && oldHeader!=header+",10PM,ExtensionMin") { f.close();return false; }
        while (f.available()) {
            String line=f.readStringUntil('\n');line.trim();
            if (!line.length() || line.startsWith(date+",")) continue;
            // Preserve every historical value; migration adds only empty cells.
            content+=line+(oldSchema ? ",,":"")+"\n";
        }
        f.close();
    }
    content+=row+"\n";
    return summaryReplace(path,content);
}
}

const char* summaryExportStatus() { return summaryHealth; }

void recordOfficialVehicle() {
    // Determine eligibility using the event's current RTC, never stale showTime.
    if (officialShowNow()) { ++totalShowCars;++officialHourly[rtc.now().hour()]; }
}

bool writeOfficialSummaries() {
    if (!countsReady() || !officialCountsComplete) { summaryHealth="INCOMPLETE_COUNTS_KEEP_FILES";return false; }
    if (SD.cardType()==CARD_NONE) { summaryHealth="SD_UNAVAILABLE";return false; }
    DateTime now=rtc.now();char date[16];
    snprintf(date,sizeof(date),"%04d-%02d-%02d",now.year(),now.month(),now.day());
    unsigned int before=0,total=0;
    for(int i=0;i<17;++i) before+=hourlyCount[i];
    for(int i=0;i<24;++i) total+=officialHourly[i];
    unsigned int b6=officialHourly[17],b7=officialHourly[18],b8=officialHourly[19],b9=officialHourly[20];
    int extension=effectiveExtension();
    // 9PM always means the hour ending at 21:00. Closing-grace cars are
    // included in DailyTotal, never mislabeled as 8-9 PM traffic. The late
    // 10PM field is populated only on explicitly extended nights.
    unsigned int c6=b6,c7=c6+b7,c8=c7+b8,c9=c8+b9;
    float sum=0;int samples=0;
    for(int i=17;i<=20;++i) if(hourlyTemp[i]!=0) {sum+=hourlyTemp[i];++samples;}
    float average=samples ? sum/samples:0;
    char suffix[48];
    if(extension) snprintf(suffix,sizeof(suffix),",%u,%d",officialHourly[21],extension);
    else snprintf(suffix,sizeof(suffix),",,");
    char cumulativeSuffix[48];
    if (extension) snprintf(cumulativeSuffix,sizeof(cumulativeSuffix),",%u,%d",c9+officialHourly[21],extension);
    else snprintf(cumulativeSuffix,sizeof(cumulativeSuffix),",,");
    char cumulative[256],buckets[256];
    snprintf(cumulative,sizeof(cumulative),"%s,%d,%u,%u,%u,%u,%u,%d,%.1f%s",
        date,recordingDaysRunning(),before,c6,c7,c8,c9,totalShowCars,average,cumulativeSuffix);
    snprintf(buckets,sizeof(buckets),"%s,%d,%u,%u,%u,%u,%u,%d,%.1f%s",
        date,recordingDaysRunning(),b6,b7,b8,b9,total,totalShowCars,average,suffix);
    // Both counters now provide the same cumulative AND hourly metrics schemas.
    bool a=upsertSummary(summaryPath("ShowSummary.csv"),
        "Date,DaysRunning,Before5,6PM,7PM,8PM,9PM,ShowTotal,DailyAvgTemp",date,cumulative);
    bool b=upsertSummary(summaryPath("ShowSummaryBuckets.csv"),
        "Date,DaysRunning,6PM,7PM,8PM,9PM,DailyTotal,ShowTotal,DailyAvgTemp",date,buckets);
    summaryHealth=a && b ? "OK":"WRITE_FAILED_RETRY";
    if (a && b) publishMQTT(MQTT_PUB_SUMMARY,cumulative);
    return a && b;
}
