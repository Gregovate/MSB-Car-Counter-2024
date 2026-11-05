#include "traffic/Firmware.h"
#include <Preferences.h>

namespace {
traffic::OperatingSettings settings;
Preferences settingsPrefs;
String settingsHealth="DEFAULTS_NOT_YET_CONFIGURED";
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
}

int effectiveExtension() { return traffic::extensionForDate(settings,settingsToday()); }
int effectiveShowEnd() { return traffic::officialEnd(settings,settingsToday()); }
bool officialShowNow() {
    if (!recordingShowSeasonActive()) return false;
    DateTime now=rtc.now();
    return traffic::insideOfficialWindow(settings,gateDevice(),settingsToday(),now.hour()*60+now.minute());
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
        DateTime now=rtc.now();char day[9];
        snprintf(day,sizeof(day),"%04d%02d%02d",now.year(),now.month(),now.day());
        int hh=stamp.substring(8,10).toInt(),mm=stamp.substring(10,12).toInt(),ss=stamp.substring(12,14).toInt();
        int age=now.hour()*3600+now.minute()*60+now.second()-(hh*3600+mm*60+ss);
        if (stamp.substring(0,8)!=day || hh>23 || mm>59 || ss>59 || age<0 || age>60 ||
            stamp<=settingsPrefs.getString("correction","")) return true;
        if (settingsPrefs.putString("correction",stamp)!=stamp.length()) return true;
        if (kind=="daily") totalDailyCars=corrected;else totalShowCars=corrected;
        persistCountState();saveDailyTotal();saveShowTotal();KeepMqttAlive();return true;
    }
    if (String(topic)!=String(topic_base_path)+"/Config/operatingSettings") return false;
    traffic::OperatingSettings proposed;
    if (!traffic::parseOperatingSettings(message,proposed,recordingYear()) ||
        proposed.issuedDate>settingsToday() || proposed.issuedDate<settings.issuedDate) {
        settingsHealth="REJECTED_INVALID_SETTINGS";publishOperatingSettings();return true;
    }
    // Once late admission begins, an extension cannot be accidentally shortened
    // by replaying an older retained snapshot; next date expires it naturally.
    if (proposed.issuedDate==settingsToday() && effectiveExtension()>proposed.extension &&
        rtc.now().hour()*60+rtc.now().minute()>=settings.end) {
        settingsHealth="REJECTED_EXTENSION_REDUCTION";publishOperatingSettings();return true;
    }
    if (proposed.extension>effectiveExtension() && rtc.now().hour()*60+rtc.now().minute()>=effectiveShowEnd()) {
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
        settings=proposed;
        bool nvsOk=settingsPrefs.putString("settings",settingsText(settings))==settingsText(settings).length();
        settingsHealth=nvsOk ? "OK":"OK_SD_NVS_FAILED";
        applyDetectorSettings();
        flagDailyShowSummarySaved=false;
    }
    publishOperatingSettings();return true;
}

void serviceShowSchedule() {
    if (settings.endDate/10000!=recordingYear()) initializeOperatingSettings();
    if (!countsReady() || !recordingShowSeasonActive()) return;
    DateTime now=rtc.now();int minute=now.hour()*60+now.minute();
    if (settingsToday()>settings.endDate || settingsToday()==settings.downDate) return;
    // Reset exactly at HA's requested minute, with a durable once-per-date
    // marker. Never zero recovered counts merely because boot occurs later.
    if (minute==settings.start+settings.resetOffset && !flagDailyShowStartReset)
        resetCountsForShow();
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
