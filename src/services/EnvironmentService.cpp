#include "traffic/Firmware.h"
#include "traffic/EnvironmentPolicy.h"
#include "traffic/EnvironmentCheckpoint.h"
#include <Preferences.h>

namespace {
traffic::EnvironmentSamples environmentSamples;
void publishCompletedEnvironmentHour(int hour,double sum,unsigned count) {
    if (!count || hour<0) return;
    char topic[96];
    snprintf(topic,sizeof(topic),"%s/Env/temp/hourly/%02d",topic_base_path,hour);
    publishMQTT(topic,String(sum/count,1),false);
    // Keep existing Gate hourly subscribers alive during topic migration.
    char legacy[120];snprintf(legacy,sizeof(legacy),"%s/hourly/%02d",MQTT_PUB_TEMP,hour);
    if(String(topic)!=String(legacy)) publishMQTT(legacy,String(sum/count,1),false);
}
}

namespace {
Preferences environmentPrefs;
bool environmentLoaded=false,environmentPrefsReady=false,environmentClockBlocked=false;
uint32_t environmentSequence=0;
int environmentSlot=-1,environmentBlockedDate=0,environmentBlockedHour=-1;
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
    }else environmentPersistence="NEW_DAY";
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
        publishCompletedEnvironmentHour(previousHour,sum,count);
        if(previousDate!=date) for(float& value:hourlyTemp) value=NAN;
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
