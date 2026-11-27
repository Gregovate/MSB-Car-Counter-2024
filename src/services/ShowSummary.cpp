#include "traffic/Firmware.h"
#include <stdlib.h>
#include <limits.h>
#include "traffic/NightlyCsv.h"

namespace {
const char* summaryHealth="NOT_DUE";
// CSV headers and legacy MQTT payloads are independent contracts.
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
String summaryCell(const String& line,unsigned column) {
    int start=0;
    for(unsigned i=0;i<column;++i){int end=line.indexOf(',',start);if(end<0)return "";start=end+1;}
    int end=line.indexOf(',',start);return end<0?line.substring(start):line.substring(start,end);
}
int summaryColumn(const String& header,const char* name) {
    for(int i=0;;++i) {
        String cell=summaryCell(header,i);
        if(cell==name)return i;
        if(!cell.length())return -1;
    }
}
// Resolve historical cells by name. A missing column or date stays blank.
String savedSummaryCell(const String& path,const String& date,const char* name) {
    File f=recordingOpen(path,FILE_READ);
    if(!f || f.size()>65536){f.close();return "";}
    String header=f.readStringUntil('\n');header.trim();
    int column=summaryColumn(header,name);
    while(column>=0 && f.available()) {
        String line=f.readStringUntil('\n');line.trim();
        if(line.startsWith(date+",")){f.close();return summaryCell(line,column);}
    }
    f.close();return "";
}
String savedSummaryTemperature(const String& path,const String& date) {
    String text=savedSummaryCell(path,date,"DailyAvgTemp");text.trim();
    char* end=nullptr;
    float value=strtof(text.c_str(),&end);
    if(text.length() && end && !*end && isfinite(value) && value>=-40 && value<=120)return text;
    return "";
}
bool parseHistoricalCount(const String& cell,long& value) {
    value=0;
    if(!cell.length())return false;
    for(unsigned i=0;i<cell.length();++i) {
        int digit=cell[i]-'0';
        if(digit<0 || digit>9 || value>(LONG_MAX-digit)/10)return false;
        value=value*10+digit;
    }
    return true;
}
String migratedRow(const String& line,bool buckets) {
    // Both legacy formats have nine common-position cells and optional 10PM/extension.
    const int firstHour=buckets?2:3;
    String result=summaryCell(line,0);
    for(int i=1;i<firstHour+4;++i)result+=","+summaryCell(line,i);
    String daily,late;
    long total=0;
    if(buckets) {
        daily=summaryCell(line,6);
        bool valid=parseHistoricalCount(daily,total);
        for(int i=2;valid && i<6;++i) {
            long count;
            valid=parseHistoricalCount(summaryCell(line,i),count) && count<=total;
            if(valid)total-=count;
        }
        if(valid){char count[24];snprintf(count,sizeof(count),"%ld",total);late=count;}
    } else {
        // Old cumulative files lacked DailyTotal. Match the sibling sheet by date.
        String path=summaryPath("ShowSummaryBuckets.csv");
        if(!SD.exists(path))path+=".previous";
        daily=savedSummaryCell(path,summaryCell(line,0),"DailyTotal");
        long atNine;
        if(parseHistoricalCount(daily,total) && parseHistoricalCount(summaryCell(line,6),atNine) && total>=atNine)late=daily;
        else daily="";
    }
    return result+","+late+","+daily+","+summaryCell(line,7)+","+summaryCell(line,8)+","+summaryCell(line,10);
}
bool preserveLegacy(File& f,const String& path) {
    // Keep the original bytes independently of the rotating .previous backup.
    const String backup=path+".legacy";
    const bool create=!SD.exists(backup);
    const String candidate=create ? String(backup+".pending") : backup;
    uint8_t source[256],copy[256];
    if(create) {
        if(!f.seek(0))return false;
        File out=recordingOpen(candidate,FILE_WRITE);
        if(!out)return false;
        while(f.position()<f.size()) {
            size_t n=f.read(source,sizeof(source));
            if(!n || out.write(source,n)!=n){out.close();return false;}
        }
        out.flush();out.close();
    }
    File check=recordingOpen(candidate,FILE_READ);
    if(!check || check.size()!=f.size() || !f.seek(0)){check.close();return false;}
    while(f.position()<f.size()) {
        size_t n=f.read(source,sizeof(source));
        if(!n || check.read(copy,n)!=n || memcmp(source,copy,n)){check.close();return false;}
    }
    check.close();
    return !create || recordingRename(candidate,backup);
}
bool legacyReportHeader(const String& header,bool buckets) {
    String legacy=buckets?"Date,DaysRunning,6PM,7PM,8PM,9PM,DailyTotal,ShowTotal,DailyAvgTemp":"Date,DaysRunning,Before5,6PM,7PM,8PM,9PM,ShowTotal,DailyAvgTemp";
    return header==legacy || header==legacy+",10PM,ExtensionMin";
}

bool upsertSummary(const String& path,bool buckets,const String& date,const String& row) {
    String header=buckets?NightlyCsv::buckets:NightlyCsv::cumulative;
    // Recovery precedes any initialization. The prior complete CSV survives
    // power loss during replacement; retries update a date rather than duplicate it.
    if (!SD.exists(path) && SD.exists(path+".previous") && !recordingRename(path+".previous",path)) return false;
    String content=header+"\n";
    File f=recordingOpen(path,FILE_READ);
    if (!f && SD.exists(path)) return false;
    if (f) {
        // Bound memory use; refuse to truncate an unexpectedly large metrics file.
        if (f.size()>65536) { f.close();return false; }
        String oldHeader=f.readStringUntil('\n');oldHeader.trim();
        bool migrate=oldHeader!=header;
        if(migrate) {
            if(!legacyReportHeader(oldHeader,buckets) || !preserveLegacy(f,path) || !f.seek(0)){f.close();return false;}
            f.readStringUntil('\n');
        }
        while (f.available()) {
            String line=f.readStringUntil('\n');line.trim();
            if (!line.length() || line.startsWith(date+",")) continue;
            content+=(migrate ? migratedRow(line,buckets) : line)+"\n";
        }
        f.close();
    }
    content+=row+"\n";
    return summaryReplace(path,content);
}
}

// Serve a read-only view before the next nightly write migrates the SD files.
int readNightlyReport(bool buckets,String& csv) {
    if (!sdAvailable) return 503;
    String path=summaryPath(buckets ? "ShowSummaryBuckets.csv" : "ShowSummary.csv");
    if (!SD.exists(path)) {
        path+=".previous";
        if (!SD.exists(path)) return 404;
    }
    File file=recordingOpen(path,FILE_READ);
    if (!file || file.size()>65536) return 503;
    String header=file.readStringUntil('\n');
    header.trim();
    const char* current=buckets ? NightlyCsv::buckets : NightlyCsv::cumulative;
    bool legacy=header!=current;
    if (legacy && !legacyReportHeader(header,buckets)) return 422;
    csv=current;
    csv+="\n";
    while (file.available()) {
        size_t start=file.position();
        String line=file.readStringUntil('\n');
        if (file.position()<=start) return 503;
        line.trim();
        if (!line.length()) continue;
        String row=legacy ? migratedRow(line,buckets) : line;
        size_t expected=csv.length()+row.length()+1;
        csv+=row;
        csv+="\n";
        if (!row.length() || csv.length()!=expected) return 503;
    }
    return file.position()==file.size() ? 200 : 503;
}

const char* summaryExportStatus() { return summaryHealth; }

void recordOfficialVehicle() {
    // Determine eligibility using the event's current RTC, never stale showTime.
    if (productionActive() || officialShowNow()) { ++totalShowCars;++officialHourly[countEventTime().hour()]; }
}

bool writeOfficialSummaries() {
    if (!countsReady() || !officialCountsComplete) { summaryHealth="INCOMPLETE_COUNTS_KEEP_FILES";return false; }
    if (SD.cardType()==CARD_NONE) { summaryHealth="SD_UNAVAILABLE";return false; }
    DateTime now=rtc.now();char date[16];
    snprintf(date,sizeof(date),"%04d-%02d-%02d",now.year(),now.month(),now.day());
    unsigned int before=0,total=0;
    for(int i=0;i<17;++i) before+=(productionActive() ? officialHourly[i] : hourlyCount[i]);
    for(int i=0;i<24;++i) total+=officialHourly[i];
    unsigned int b6=officialHourly[17],b7=officialHourly[18],b8=officialHourly[19],b9=officialHourly[20];
    int extension=effectiveExtension();
    unsigned int late=0;
    for(int hour=21;hour<24;++hour)late+=officialHourly[hour];
    unsigned int c6=b6,c7=c6+b7,c8=c7+b8,c9=c8+b9;
    // Missing is blank, not 0 F. Weight all valid official-window samples equally.
    float average=0;char averageText[24]="";
    if(showTemperatureAverage(average)) snprintf(averageText,sizeof(averageText),"%.1f",average);
    else {
        // Reboot/no valid samples must not erase an already exported average.
        // Same date and validated schema/value only; never substitute today's
        // live reading or another counter's enclosure temperature for a night.
        for(const char* name:{"ShowSummary.csv","ShowSummary.csv.previous","ShowSummaryBuckets.csv","ShowSummaryBuckets.csv.previous"}) {
            String saved=savedSummaryTemperature(summaryPath(name),String(date));
            if(saved.length()){snprintf(averageText,sizeof(averageText),"%s",saved.c_str());break;}
        }
    }
    char cumulative[256],buckets[256];
    snprintf(cumulative,sizeof(cumulative),"%s,%d,%u,%u,%u,%u,%u,%u,%u,%d,%s,%d",
        date,recordingDaysRunning(),before,c6,c7,c8,c9,total,total,totalShowCars,averageText,extension);
    snprintf(buckets,sizeof(buckets),"%s,%d,%u,%u,%u,%u,%u,%u,%d,%s,%d",
        date,recordingDaysRunning(),b6,b7,b8,b9,late,total,totalShowCars,averageText,extension);
    bool a=upsertSummary(summaryPath("ShowSummary.csv"),false,date,cumulative);
    bool b=upsertSummary(summaryPath("ShowSummaryBuckets.csv"),true,date,buckets);
    summaryHealth=a && b ? "OK":"WRITE_FAILED_RETRY";
    if(a && b) {
        // HA consumers still receive the original cumulative format, including its optional hour 21.
        char legacy[256],suffix[40]=",";
        if(extension)snprintf(suffix,sizeof(suffix),"%u,%d",c9+officialHourly[21],extension);
        snprintf(legacy,sizeof(legacy),"%s,%d,%u,%u,%u,%u,%u,%d,%s,%s",date,
            recordingDaysRunning(),before,c6,c7,c8,c9,totalShowCars,averageText,suffix);
        publishMQTT(MQTT_PUB_SUMMARY,legacy);
    }
    return a && b && commitClosingCheckpoint();
}
