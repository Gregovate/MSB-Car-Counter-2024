#include "traffic/Firmware.h"
#include "traffic/EnvironmentPolicy.h"
#include "traffic/EnvironmentCheckpoint.h"
#include <Preferences.h>

void publishEnvironmentHourlyState();
void rememberCompletedEnvironmentHour(int date,int hour,double sum,unsigned count);

namespace {
traffic::EnvironmentSamples environmentSamples;

}

namespace {
Preferences environmentPrefs;
bool environmentLoaded=false,environmentPrefsReady=false,environmentClockBlocked=false;
uint32_t environmentSequence=0;
int environmentSlot=-1,environmentBlockedDate=0,environmentBlockedHour=-1;
struct RollingEnvironmentHour {uint32_t magic=0,date=0,hour=0;float average=0;uint32_t checksum=0;};
RollingEnvironmentHour rollingEnvironment[24]{};
bool rollingEnvironmentDirty[24]{};
uint32_t rollingChecksum(const RollingEnvironmentHour& value) {
    const auto* bytes=reinterpret_cast<const uint8_t*>(&value);uint32_t h=2166136261U;
    for(size_t i=0;i<offsetof(RollingEnvironmentHour,checksum);++i)h=(h^bytes[i])*16777619U;
    return h;
}
bool validRollingHour(const RollingEnvironmentHour& value) {
    return value.magic==0x454e4831U && value.hour<24 &&
        traffic::validDate(value.date/10000,value.date/100%100,value.date%100) &&
        isfinite(value.average) && value.average>=-40 && value.average<=120 && value.checksum==rollingChecksum(value);
}
const char* environmentPersistence="NOT_LOADED";
bool officialTemperatureSource(){return String(THIS_MQTT_CLIENT)=="espCarCounter";}
bool readEnvironmentCheckpoint(int slot,traffic::EnvironmentCheckpoint& value) {
    const char* key=slot?"b":"a";
    return environmentPrefs.getBytesLength(key)==sizeof(value) &&
        environmentPrefs.getBytes(key,&value,sizeof(value))==sizeof(value) &&
        traffic::validEnvironmentCheckpoint(value);
}
void loadEnvironmentSamples() {
    if(!rtcReady)return;
    DateTime now=rtc.now();if(!now.isValid() || rtc.lostPower() || now.year()!=recordingYear())return;
    if(environmentLoaded) {
        if(!environmentClockBlocked)return;
        const int date=now.year()*10000+now.month()*100+now.day();
        if(date<environmentBlockedDate || (date==environmentBlockedDate && now.hour()<environmentBlockedHour))return;
        environmentLoaded=false;environmentClockBlocked=false; // Clock caught up: restore once, without a reboot.
    }
    environmentLoaded=true;
    if(!officialTemperatureSource()){environmentPersistence="BACKUP_SENSOR_RAM_ONLY";return;}
    environmentPrefsReady=environmentPrefs.begin("traffic-env",false);
    if(!environmentPrefsReady){environmentPersistence="NVS_UNAVAILABLE";return;}
    for(int hour=0;hour<24;++hour){
        char key[8];snprintf(key,sizeof(key),"h%02d",hour);RollingEnvironmentHour saved{};
        if(environmentPrefs.getBytesLength(key)==sizeof(saved) &&
           environmentPrefs.getBytes(key,&saved,sizeof(saved))==sizeof(saved) &&
           validRollingHour(saved) && saved.hour==static_cast<uint32_t>(hour))rollingEnvironment[hour]=saved;
    }
    traffic::EnvironmentCheckpoint a{},b{};bool va=readEnvironmentCheckpoint(0,a),vb=readEnvironmentCheckpoint(1,b);
    if(!va && !vb){environmentPersistence="NO_SAVED_SAMPLES";return;}
    const auto& saved=vb && (!va || b.sequence>a.sequence)?b:a;
    environmentSlot=&saved==&b?1:0;environmentSequence=saved.sequence;
    const int today=now.year()*10000+now.month()*100+now.day();
    if(saved.samples.date>today || (saved.samples.date==today && saved.samples.hour>now.hour())) {
        environmentBlockedDate=saved.samples.date;environmentBlockedHour=saved.samples.hour;
        environmentClockBlocked=true;environmentPersistence="CLOCK_BEHIND_SAVED_SAMPLES";return;
    }
    if(saved.samples.date==today){
        environmentSamples=saved.samples;
        for(int i=0;i<24;++i)hourlyTemp[i]=environmentSamples.count[i]?
            environmentSamples.sum[i]/environmentSamples.count[i]:NAN;
        environmentPersistence="RESTORED";
    }else {
        for(int hour=0;hour<24;++hour)rememberCompletedEnvironmentHour(saved.samples.date,hour,saved.samples.sum[hour],saved.samples.count[hour]);
        environmentPersistence="NEW_DAY";
    }
}
void persistEnvironmentSamples() {
    if(!officialTemperatureSource() || !environmentPrefsReady || environmentClockBlocked)return;
    if(environmentSequence==UINT32_MAX){environmentPersistence="SEQUENCE_FULL";return;}
    traffic::EnvironmentCheckpoint next{};next.magic=0x454e5631U;next.sequence=environmentSequence+1;
    next.samples=environmentSamples;next.checksum=traffic::environmentChecksum(next);
    if(!traffic::validEnvironmentCheckpoint(next)){environmentPersistence="INVALID_SAMPLES";return;}
    const int slot=environmentSlot==0?1:0;const char* key=slot?"b":"a";
    traffic::EnvironmentCheckpoint check{};
    if(environmentPrefs.putBytes(key,&next,sizeof(next))!=sizeof(next) ||
       !readEnvironmentCheckpoint(slot,check) || memcmp(&next,&check,sizeof(next))) {
        environmentPersistence="WRITE_VERIFY_FAILED";return;
    }
    environmentSequence=next.sequence;environmentSlot=slot;environmentPersistence="OK";
}
}
const char* environmentPersistenceStatus(){return environmentPersistence;}

void serviceEnvironmentHour() {
    if (!rtcReady) return;
    loadEnvironmentSamples();
    if(environmentClockBlocked || rtc.lostPower() || rtc.now().year()!=recordingYear())return;
    DateTime now=rtc.now();
    int date=now.year()*10000+now.month()*100+now.day();
    int previousDate=environmentSamples.date,previousHour=environmentSamples.hour;
    double sum=previousHour>=0 ? environmentSamples.sum[previousHour]:0;
    unsigned count=previousHour>=0 ? environmentSamples.count[previousHour]:0;
    if(!environmentSamples.advance(date,now.hour())) return;
    if(previousDate!=date || previousHour!=now.hour()) {
        // Close the old bucket before any new-hour reading, even during failures.
        rememberCompletedEnvironmentHour(previousDate,previousHour,sum,count);
        if(previousDate!=date) {
            for(float& value:hourlyTemp) value=NAN;
            // Daily sums reset, but dated completed-hour slots roll across midnight.
        }
        publishEnvironmentHourlyState();
    }
}

void averageHourlyTemp() {
    serviceEnvironmentHour();
    if(!rtcReady || environmentClockBlocked || rtc.lostPower() || rtc.now().year()!=recordingYear()) return;
    DateTime now=rtc.now();int date=now.year()*10000+now.month()*100+now.day();
    if(environmentSamples.add(date,now.hour(),tempF,humidity,officialShowNow())) {
        hourlyTemp[now.hour()]=environmentSamples.sum[now.hour()]/environmentSamples.count[now.hour()];
        persistEnvironmentSamples(); // One verified NVS snapshot per accepted minute sample.
    }
}

bool showTemperatureAverage(float& average) {
    serviceEnvironmentHour();
    if(environmentClockBlocked || !environmentSamples.showCount) return false;
    average=environmentSamples.showSum/environmentSamples.showCount;return true;
}


// Completed-hour slots are a rolling last-24-hours history, not a calendar-day
// array. Preserve full dates across midnight; Car also persists each slot in NVS.
void rememberCompletedEnvironmentHour(int date,int hour,double sum,unsigned count) {
    if(!count || hour<0 || hour>23 || !traffic::validDate(date/10000,date/100%100,date%100))return;
    RollingEnvironmentHour next{};next.magic=0x454e4831U;next.date=date;next.hour=hour;next.average=sum/count;
    next.checksum=rollingChecksum(next);if(!validRollingHour(next))return;
    const auto& old=rollingEnvironment[hour];
    if(validRollingHour(old) && (old.date>next.date || (old.date==next.date && old.average==next.average && !rollingEnvironmentDirty[hour])))return;
    rollingEnvironment[hour]=next;rollingEnvironmentDirty[hour]=officialTemperatureSource();
    if(officialTemperatureSource() && environmentPrefsReady) {
        char key[8];snprintf(key,sizeof(key),"h%02d",hour);RollingEnvironmentHour check{};
        if(environmentPrefs.putBytes(key,&next,sizeof(next))!=sizeof(next) ||
           environmentPrefs.getBytes(key,&check,sizeof(check))!=sizeof(check) ||
           memcmp(&next,&check,sizeof(next)))environmentPersistence="HOURLY_WRITE_VERIFY_FAILED";
        else rollingEnvironmentDirty[hour]=false;
    }
}

void publishEnvironmentHourlyState() {
    loadEnvironmentSamples();
    if(!rtcReady || rtc.lostPower() || environmentClockBlocked)return;
    DateTime now=rtc.now();if(!now.isValid() || now.year()!=recordingYear())return;
    const int today=now.year()*10000+now.month()*100+now.day();
    // Seed completed buckets from the .28 daily checkpoint without changing its schema.
    if(environmentSamples.date==today)for(int hour=0;hour<now.hour();++hour)
        rememberCompletedEnvironmentHour(today,hour,environmentSamples.sum[hour],environmentSamples.count[hour]);
    const uint32_t current=now.unixtime();
    for(int hour=0;hour<24;++hour) {
        const auto& saved=rollingEnvironment[hour];bool known=validRollingHour(saved);
        uint32_t end=known?DateTime(saved.date/10000,saved.date/100%100,saved.date%100,hour).unixtime()+3600U:0;
        known=known && end<=current && current-end<86400U;
        if(known && rollingEnvironmentDirty[hour])rememberCompletedEnvironmentHour(saved.date,hour,saved.average,1);
        String value=known?String(saved.average,1):String("");
        String date=known?String(saved.date):String("");
        char topic[120],legacy[120];
        snprintf(topic,sizeof(topic),"%s/Env/temp/hourly/%02d",topic_base_path,hour);
        snprintf(legacy,sizeof(legacy),"%s/hourly/%02d",MQTT_PUB_TEMP,hour);
        publishMQTT(topic,value,true);if(String(topic)!=String(legacy))publishMQTT(legacy,value,true);
        snprintf(topic,sizeof(topic),"%s/Env/temp/hourlyDate/%02d",topic_base_path,hour);
        snprintf(legacy,sizeof(legacy),"%s/hourlyDate/%02d",MQTT_PUB_TEMP,hour);
        publishMQTT(topic,date,true);if(String(topic)!=String(legacy))publishMQTT(legacy,date,true);
    }
}
