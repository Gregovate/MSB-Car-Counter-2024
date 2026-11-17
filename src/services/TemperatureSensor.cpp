#include "traffic/Firmware.h"
#include "traffic/EnvironmentPolicy.h"

namespace {
bool dhtAttempted=false,dhtHasValidReading=false,dhtCurrentValid=false;
unsigned long lastDhtAttempt=0,lastDhtSuccess=0;
unsigned long dhtFailures=0,dhtConsecutiveFailures=0;
const char* dhtHealth="WARMING_UP";
}

void publishEnvironmentState() {
    // Null means unavailable. Never publish internal sentinel temperatures to HA.
    bool fresh=dhtHasValidReading && dhtCurrentValid && millis()-lastDhtSuccess<=120000UL;
    char payload[100];
    if(fresh) snprintf(payload,sizeof(payload),"{\"tempF\": %.1f, \"humidity\": %.1f}",tempF,humidity);
    else snprintf(payload,sizeof(payload),"{\"tempF\": null, \"humidity\": null}");
    publishMQTT(MQTT_PUB_TEMP,String(payload),true);
    char diagnostic[256];
    snprintf(diagnostic,sizeof(diagnostic),
        "{\"status\":\"%s\",\"failures\":%lu,\"consecutiveFailures\":%lu,\"hasValidReading\":%s,\"lastSuccessUptimeMs\":%lu}",
        fresh ? "OK":dhtCurrentValid ? "STALE":dhtHealth,dhtFailures,dhtConsecutiveFailures,
        dhtHasValidReading ? "true":"false",lastDhtSuccess);
    publishMQTT((String(topic_base_path)+"/System/dhtHealth").c_str(),String(diagnostic),true);
}

void readTempandRH() {
    // Hour closure is independent of sensor success and never blocks counting.
    serviceEnvironmentHour();
    unsigned long now=millis();
    if(now<60000UL && !dhtAttempted) return; // First read after one minute of uptime.
    if(dhtAttempted && now-lastDhtAttempt<60000UL) return;
    dhtAttempted=true;lastDhtAttempt=now;
    // Validate a complete pair before replacing the last accepted sample.
    float rh=dht.readHumidity(),temperature=dht.readTemperature(true);
    if(!traffic::validEnvironment(temperature,rh)) {
        ++dhtFailures;++dhtConsecutiveFailures;dhtCurrentValid=false;
        dhtHealth=(!isfinite(temperature) || !isfinite(rh)) ? "READ_FAILED":"OUT_OF_RANGE";
        if(dhtConsecutiveFailures==1) publishDebugLog("DHT reading invalid; measurements unavailable.");
        publishEnvironmentState();return;
    }
    bool recovered=!dhtCurrentValid;
    tempF=temperature;humidity=rh;dhtHasValidReading=true;dhtCurrentValid=true;
    lastDhtSuccess=now;dhtConsecutiveFailures=0;dhtHealth="OK";
    averageHourlyTemp(); // Exactly once per new valid sensor sample.
    if(recovered) {
        publishDebugLog("DHT valid reading available.");
        publishEnvironmentState();lastTempPubMillis=now;
        Serial.printf("DHT valid: %.1f F, %.1f %% RH\n",tempF,humidity);
    }
}
