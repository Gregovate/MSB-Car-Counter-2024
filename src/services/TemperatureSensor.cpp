#include "traffic/Firmware.h"
#include "traffic/EnvironmentPolicy.h"
#include "traffic/DhtCompatibility.h"

namespace {
bool dhtAttempted=false,dhtHasValidReading=false,dhtCurrentValid=false;
unsigned long lastDhtAttempt=0,lastDhtSuccess=0,lastEnvironmentPublish=0;
unsigned long dhtFailures=0,dhtConsecutiveFailures=0;
const char* dhtHealth="WARMING_UP";
float lastRawTemperature=NAN,lastRawHumidity=NAN;
const char* dhtRejection="NOT_READ";
bool dhtEncodingCorrected=false;
void formatDhtNumber(float value,char (&out)[24]) {
    if(isfinite(value))snprintf(out,sizeof(out),"%.6g",value);
    else snprintf(out,sizeof(out),"null"); // Never emit NaN/Infinity as JSON numbers.
}
}

void publishEnvironmentState() {
    // Null means unavailable. Never publish internal sentinel temperatures to HA.
    bool fresh=dhtHasValidReading && dhtCurrentValid && millis()-lastDhtSuccess<=120000UL;
    char payload[100];
    if(fresh) snprintf(payload,sizeof(payload),"{\"tempF\": %.1f, \"humidity\": %.1f}",tempF,humidity);
    else snprintf(payload,sizeof(payload),"{\"tempF\": null, \"humidity\": null}");
    publishMQTT(MQTT_PUB_TEMP,String(payload),true);
    lastEnvironmentPublish=millis();
    char rawTemperature[24],rawHumidity[24];
    formatDhtNumber(lastRawTemperature,rawTemperature);formatDhtNumber(lastRawHumidity,rawHumidity);
    char diagnostic[512];
    snprintf(diagnostic,sizeof(diagnostic),
        "{\"status\":\"%s\",\"failures\":%lu,\"consecutiveFailures\":%lu,\"hasValidReading\":%s,\"lastSuccessUptimeMs\":%lu,"
        "\"pin\":%d,\"type\":\"DHT%d\",\"lastReadUptimeMs\":%lu,\"lastReadTempF\":%s,\"lastReadHumidity\":%s,\"rejection\":\"%s\",\"encoding\":\"%s\"}",
        fresh ? "OK":dhtCurrentValid ? "STALE":dhtHealth,dhtFailures,dhtConsecutiveFailures,
        dhtHasValidReading ? "true":"false",lastDhtSuccess,DHTPIN,DHTTYPE,lastDhtAttempt,rawTemperature,rawHumidity,dhtRejection,dhtEncodingCorrected?"TWOS_COMPLEMENT_COMPAT":"STANDARD");
    publishMQTT((String(topic_base_path)+"/System/dhtHealth").c_str(),String(diagnostic),true);
}

void readTempandRH() {
    // Hour closure is independent of sensor success and never blocks counting.
    serviceEnvironmentHour();
    unsigned long now=millis();
    if(now-lastEnvironmentPublish>=600000UL) publishEnvironmentState();
    if(now<60000UL && !dhtAttempted) return; // First read after one minute of uptime.
    if(dhtAttempted && now-lastDhtAttempt<60000UL) return;
    dhtAttempted=true;lastDhtAttempt=now;
    // Validate a complete pair before replacing the last accepted sample.
    float rh=dht.readHumidity(),temperature=dht.readTemperature(true);
    lastRawTemperature=temperature;lastRawHumidity=rh;
    temperature=traffic::compatibleDht22F(temperature,dhtEncodingCorrected);
    if(!traffic::validEnvironment(temperature,rh)) {
        ++dhtFailures;++dhtConsecutiveFailures;dhtCurrentValid=false;
        dhtHealth=(!isfinite(temperature) || !isfinite(rh)) ? "READ_FAILED":"OUT_OF_RANGE";
        bool badTemperature=!isfinite(temperature) || temperature< -40 || temperature>120;
        bool badHumidity=!isfinite(rh) || rh<0 || rh>100;
        dhtRejection=badTemperature && badHumidity ? "BOTH_INVALID":
            badTemperature ? (isfinite(temperature)?"TEMPERATURE_RANGE":"TEMPERATURE_READ_FAILED"):
            (isfinite(rh)?"HUMIDITY_RANGE":"HUMIDITY_READ_FAILED");
        char t[24],h[24];formatDhtNumber(temperature,t);formatDhtNumber(rh,h);
        if(dhtConsecutiveFailures==1) {
            char message[160];snprintf(message,sizeof(message),"DHT invalid: GPIO %d, tempF=%s, humidity=%s, reason=%s",DHTPIN,t,h,dhtRejection);
            publishDebugLog(message);
        }
        Serial.printf("DHT invalid: GPIO %d, tempF=%s, humidity=%s, reason=%s\n",DHTPIN,t,h,dhtRejection);
        publishEnvironmentState();return;
    }
    dhtRejection="NONE";
    bool recovered=!dhtCurrentValid;
    tempF=temperature;humidity=rh;dhtHasValidReading=true;dhtCurrentValid=true;
    lastDhtSuccess=now;dhtConsecutiveFailures=0;dhtHealth="OK";
    averageHourlyTemp(); // Exactly once per new valid sensor sample.
    if(recovered) {
        publishDebugLog("DHT valid reading available.");
        publishEnvironmentState();
        Serial.printf("DHT valid: %.1f F, %.1f %% RH\n",tempF,humidity);
    }
}
