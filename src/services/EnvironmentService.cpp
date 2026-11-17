#include "traffic/Firmware.h"
#include "traffic/EnvironmentPolicy.h"

namespace {
traffic::EnvironmentSamples environmentSamples;
void publishCompletedEnvironmentHour(int hour,double sum,unsigned count) {
    if (!count || hour<0) return;
    char topic[96];
    snprintf(topic,sizeof(topic),"%s/hourly/%02d",MQTT_PUB_TEMP,hour);
    publishMQTT(topic,String(sum/count,1),false);
}
}

void serviceEnvironmentHour() {
    if (!rtcReady) return;
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
    if(!rtcReady) return;
    DateTime now=rtc.now();int date=now.year()*10000+now.month()*100+now.day();
    if(environmentSamples.add(date,now.hour(),tempF,humidity,officialShowNow()))
        hourlyTemp[now.hour()]=environmentSamples.sum[now.hour()]/environmentSamples.count[now.hour()];
}

bool showTemperatureAverage(float& average) {
    serviceEnvironmentHour();
    if(!environmentSamples.showCount) return false;
    average=environmentSamples.showSum/environmentSamples.showCount;return true;
}
