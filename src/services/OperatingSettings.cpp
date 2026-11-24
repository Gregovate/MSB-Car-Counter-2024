#include "traffic/Firmware.h"
#include <Preferences.h>

namespace {
traffic::OperatingSettings settings;
Preferences settingsPrefs;
int resetArmedDate=0;
String settingsHealth="DEFAULTS_NOT_YET_CONFIGURED";
// Preserve the last event after health recovers; broker retention survives reconnects.
String lastSettingsRejection;
uint32_t settingsRejectionCount=0;
void recordSettingsRejection(const char* reason,int field,int receivedDate,const DateTime& now) {
    char diagnostic[480];
    snprintf(diagnostic,sizeof(diagnostic),
        "{\"reason\":\"%s\",\"field\":%d,\"receivedIssuedDate\":%d,"
        "\"acceptedIssuedDate\":%d,\"recordingYear\":%d,\"rtcReady\":%s,"
        "\"rtcLocal\":\"%04d-%02d-%02d %02d:%02d:%02d\",\"uptimeMs\":%lu,\"rejectionCount\":%lu}",
        reason,field,receivedDate,settings.issuedDate,recordingYear(),rtcReady ? "true":"false",
        now.year(),now.month(),now.day(),now.hour(),now.minute(),now.second(),
        static_cast<unsigned long>(millis()),static_cast<unsigned long>(++settingsRejectionCount));
    lastSettingsRejection=diagnostic;
    Serial.print("Operating settings rejected: ");Serial.println(lastSettingsRejection);
}
bool gateDevice() { return String(THIS_MQTT_CLIENT)=="espGateCounter"; }
int settingsToday() {
    DateTime n=rtc.now();
    return n.year()*10000+n.month()*100+n.day();
}
String settingsText(const traffic::OperatingSettings& s) {
    char b[180];
    snprintf(b,sizeof(b),"1,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        s.start,s.end,s.resetOffset,s.grace,s.extension,s.issuedDate,
        s.alarmMs,s.detectMs,s.endDate,s.downDate,s.issuedDate/10000);
    return String(b);
}
bool readSettingsCopy(const char* path,traffic::OperatingSettings& out) {
    File f=recordingOpen(path,FILE_READ);
    if (!f || f.size()>180) { f.close();return false; }
    size_t size=f.size();String value=f.readString();f.close();
    return value.length()==size && traffic::parseOperatingSettings(value.c_str(),out,recordingYear());
}
bool writeSettingsCopy(const char* path,const traffic::OperatingSettings& value) {
    String text=settingsText(value);
    File f=recordingOpen(path,FILE_WRITE);
    size_t written=f ? f.print(text):0;f.flush();f.close();
    traffic::OperatingSettings check;
    return written==text.length() && readSettingsCopy(path,check) && settingsText(check)==text;
}
void armUpcomingReset() {
    DateTime now=rtc.now();
    resetArmedDate=now.hour()*60+now.minute()<=settings.start+settings.resetOffset?settingsToday():0;
}
void applyDetectorSettings() {
    // Device-specific detector semantics remain intact; only accepted setpoints change.
    applyOperatingDetector(settings.alarmMs,settings.detectMs);
}
}

void initializeOperatingSettings() {
    settingsHealth="DEFAULTS_NOT_YET_CONFIGURED";
    settings=traffic::OperatingSettings{};
    if (gateDevice()) { settings.resetOffset=10;settings.grace=25;settings.detectMs=100; }
    settings.issuedDate=recordingYear()*10000+101;
    settings.endDate=recordingYear()*10000+1231;
    settings.downDate=recordingYear()*10000+1224;
    settingsPrefs.begin("traffic-ops",false);
    traffic::OperatingSettings saved;
    String nvs=settingsPrefs.getString("settings","");
    if (readSettingsCopy("/operating-settings.a",saved) ||
        readSettingsCopy("/operating-settings.b",saved) ||
        traffic::parseOperatingSettings(nvs.c_str(),saved,recordingYear())) {
        settings=saved;settingsHealth="OK_RESTORED";
    }
    applyDetectorSettings();
    armUpcomingReset();
}

int effectiveExtension() { return traffic::extensionForDate(settings,settingsToday()); }
int effectiveShowEnd() { return traffic::officialEnd(settings,settingsToday()); }
bool officialShowNow() {
    if (!recordingShowSeasonActive()) return false;
    DateTime now=countEventTime();
    int date=now.year()*10000+now.month()*100+now.day();
    return traffic::insideOfficialWindow(settings,gateDevice(),date,now.hour()*60+now.minute());
}

void publishOperatingSettings() {
    if (!mqttOnline()) return;
    String base=String(topic_base_path)+"/State/";
    publishMQTT((base+"operatingSettings").c_str(),settingsText(settings),true);
    publishMQTT((base+"showStartMin").c_str(),String(settings.start),true);
    publishMQTT((base+"showEndMin").c_str(),String(effectiveShowEnd()),true);
    publishMQTT((base+"resetMinute").c_str(),String(settings.start+settings.resetOffset),true);
    publishMQTT((base+"countStartMinute").c_str(),String(traffic::officialStart(settings,gateDevice())),true);
    publishMQTT((base+"countEndMinute").c_str(),String(effectiveShowEnd()),true);
    publishMQTT((base+"extensionMinutes").c_str(),String(effectiveExtension()),true);
    publishMQTT((base+"alarmTimeoutMs").c_str(),String(settings.alarmMs),true);
    publishMQTT((base+"carDetectMs").c_str(),String(settings.detectMs),true);
    // Publish evidence before health so warning consumers can retrieve its details.
    if (lastSettingsRejection.length())
        publishMQTT((String(topic_base_path)+"/System/settingsLastRejection").c_str(),lastSettingsRejection,true);
    publishMQTT((String(topic_base_path)+"/System/settingsHealth").c_str(),settingsHealth,true);
    publishMQTT((String(topic_base_path)+"/System/summaryExport").c_str(),summaryExportStatus(),true);
}

bool handleOperatingCommand(const char* topic,const char* message) {
    if (String(topic)==String(topic_base_path)+"/Config/countCorrection") {
        // Non-retained HA manual action, valid for 60 seconds, at most once.
        // Record its id before applying so broker replay cannot erase later cars.
        String request(message);
        int split=request.indexOf(',');int second=request.indexOf(',',split+1);
        if (split!=14 || second<0 || !countsReady()) return true;
        String stamp=request.substring(0,split),kind=request.substring(split+1,second);
        String value=request.substring(second+1);
        uint32_t corrected=0;
        for (unsigned int i=0;i<value.length();++i) {
            char c=value[i];
            if (c<'0' || c>'9' || corrected>(2147483647UL-(c-'0'))/10) return true;
            corrected=corrected*10+(c-'0');
        }
        if (!value.length() || (kind!="daily" && kind!="show")) return true;
        for (unsigned int i=0;i<stamp.length();++i) if(stamp[i]<'0' || stamp[i]>'9') return true;
        DateTime now=rtc.now();/* Allow full RTC component ranges; format widths are minimums. */ char day[24];
        snprintf(day,sizeof(day),"%04d%02d%02d",now.year(),now.month(),now.day());
        int hh=stamp.substring(8,10).toInt(),mm=stamp.substring(10,12).toInt(),ss=stamp.substring(12,14).toInt();
        int age=now.hour()*3600+now.minute()*60+now.second()-(hh*3600+mm*60+ss);
        if (stamp.substring(0,8)!=day || hh>23 || mm>59 || ss>59 || age<0 || age>60 ||
            stamp<=settingsPrefs.getString("correction","")) return true;
        detectCar(); // Correction supersedes every already captured passage.
        uint32_t nonce=DateTime(now.year(),now.month(),now.day(),hh,mm,ss).unixtime();
        if(!correctCountState(kind=="daily",corrected,nonce)){publishCountHealth();return true;}
        settingsPrefs.putString("correction",stamp); // Journal also retains replay identity.
        saveDailyTotal();saveShowTotal();KeepMqttAlive();return true;
    }
    if (String(topic)!=String(topic_base_path)+"/Config/operatingSettings") return false;
    traffic::OperatingSettings proposed;
    traffic::OperatingParseDiagnostic diagnostic;
    // One RTC snapshot makes the comparison and diagnostic describe the same instant.
    DateTime receivedAt=rtc.now();
    int receivedToday=receivedAt.year()*10000+receivedAt.month()*100+receivedAt.day();
    bool parsed=traffic::parseOperatingSettings(message,proposed,recordingYear(),&diagnostic);
    const char* rejection=!parsed ? diagnostic.reason :
        proposed.issuedDate>receivedToday ? "FUTURE_ISSUED_DATE" :
        proposed.issuedDate<settings.issuedDate ? "STALE_ISSUED_DATE" : nullptr;
    if (rejection) {
        recordSettingsRejection(rejection,parsed ? 6:diagnostic.field,diagnostic.issuedDate,receivedAt);
        settingsHealth="REJECTED_INVALID_SETTINGS";publishOperatingSettings();return true;
    }
    // Once late admission begins, an extension cannot be accidentally shortened
    // by replaying an older retained snapshot; next date expires it naturally.
    if (proposed.issuedDate==settingsToday() && effectiveExtension()>proposed.extension &&
        rtc.now().hour()*60+rtc.now().minute()>=settings.end) {
        recordSettingsRejection("EXTENSION_REDUCTION",5,proposed.issuedDate,receivedAt);
        settingsHealth="REJECTED_EXTENSION_REDUCTION";publishOperatingSettings();return true;
    }
    if (proposed.extension>effectiveExtension() && rtc.now().hour()*60+rtc.now().minute()>=effectiveShowEnd()) {
        recordSettingsRejection("EXTENSION_AFTER_CUTOFF",5,proposed.issuedDate,receivedAt);
        settingsHealth="REJECTED_EXTENSION_AFTER_CUTOFF";publishOperatingSettings();return true;
    }
    if (settingsText(proposed)!=settingsText(settings)) {
        // A verified SD copy must exist before acknowledging changed settings.
        // Write backup first; leave the previous primary intact on failure.
        if (!writeSettingsCopy("/operating-settings.b",proposed)) {
            settingsHealth="SAVE_FAILED_KEEP_PREVIOUS";publishOperatingSettings();return true;
        }
        if (!writeSettingsCopy("/operating-settings.a",proposed)) {
            settingsHealth="SAVE_FAILED_KEEP_PREVIOUS";publishOperatingSettings();return true;
        }
        detectCar();
        settings=proposed;
        bool nvsOk=settingsPrefs.putString("settings",settingsText(settings))==settingsText(settings).length();
        settingsHealth=nvsOk ? "OK":"OK_SD_NVS_FAILED";
        applyDetectorSettings();
        armUpcomingReset();
        flagDailyShowSummarySaved=false;
    }
    publishOperatingSettings();return true;
}

void serviceEventReset() {
    if(!countsReady() || !recordingShowSeasonActive())return;
    DateTime now=countEventTime();int date=now.year()*10000+now.month()*100+now.day();
    int minute=now.hour()*60+now.minute(),deadline=settings.start+settings.resetOffset;
    if(minute<deadline){resetArmedDate=date;return;}
    if(date==resetArmedDate && date<=settings.endDate && date!=settings.downDate && !flagDailyShowStartReset)
        resetCountsForShow();
}

void serviceShowSchedule() {
    if (settings.endDate/10000!=recordingYear()) initializeOperatingSettings();
    if (!countsReady() || !recordingShowSeasonActive()) return;
    DateTime now=rtc.now();int minute=now.hour()*60+now.minute();
    if (settingsToday()>settings.endDate || settingsToday()==settings.downDate) return;
    // A boot after the deadline never arms a retroactive reset. An uptime
    // stall may cross the minute: drain old events, then catch up this armed reset.
    detectCar();
    serviceEventReset();
    static unsigned long lastSummaryAttempt=0;
    if (minute>=effectiveShowEnd() && !flagDailyShowSummarySaved && millis()-lastSummaryAttempt>=10000UL) {
        lastSummaryAttempt=millis();
        flagDailyShowSummarySaved=writeOfficialSummaries();
    }
}

bool officialSeriesCanStart() {
    DateTime now=rtc.now();
    return !recordingShowSeasonActive() || now.hour()*60+now.minute()<traffic::officialStart(settings,gateDevice());
}

int operatingSeasonEndDate() {
    return settings.endDate/10000==recordingYear() ? settings.endDate:recordingYear()*10000+1231;
}
int operatingDownDate() {
    return settings.downDate/10000==recordingYear() ? settings.downDate:recordingYear()*10000+1224;
}
