#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.


void publishMQTT(const char *topic, const String &message, bool retainFlag) {
    if (!countTopicPublishable(topic) || !mqttOnline()) return;
    mqtt_client.publish(topic,message.c_str(),retainFlag);
    start_MqttMillis=millis();
}

void publishMQTT(const char *topic, const String &message) {
    publishMQTT(topic, message, false);
}







void KeepMqttAlive() {
    publishTimingConfig();
    publishOperatingSettings();
    publishMQTT(MQTT_PUB_ALARM, carStuckAlarmActive ? "ALARM_ENTER_STUCK" : "CLEAR", true);
    publishCountHealth();

    // ---- Heartbeat (retained, ONLY here to prevent show spam) ----
    // GAL 25-11-22.3: keep firmware visible for dashboards (retained overwrite)
    publishMQTT(MQTT_PUB_FW_VERSION, String(FWVersion), true);
    publishMQTT(
        MQTT_PUB_HEARTBEAT,
        String("{\"boot\":\"") + bootTimestamp +
        "\",\"now\":\"" + getRtcTimestamp() +
        "\",\"enter\":" + totalDailyCars +
        ",\"show\":" + totalShowCars +
        ",\"rssi\":" + WiFi.RSSI() +
        "}",
        true
    );

    // ---- Retained Temp/RH JSON ----
    char jsonPayload[100];
    snprintf(jsonPayload, sizeof(jsonPayload),
            "{\"tempF\": %.1f, \"humidity\": %.1f}", tempF, humidity);
// GAL 25-11-22.4: temp/RH publish removed from KeepMqttAlive; now on 10-min timer
// publishMQTT(MQTT_PUB_TEMP, String(jsonPayload), true);



    // ---- WiFi Diagnostics (retained) ----
    publishMQTT(MQTT_PUB_RSSI, String(WiFi.RSSI()), true);
    publishMQTT(MQTT_PUB_SSID, WiFi.SSID(),        true);
    publishMQTT(MQTT_PUB_IP,   WiFi.localIP().toString(), true);

     // ---- Retained show window + date (for HA dashboards / GateCounter offset) ----
    char showStartDateBuf[11];
    snprintf(showStartDateBuf, sizeof(showStartDateBuf), "%04d-%02d-%02d",
            showStartY, showStartM, showStartD);
    // HA owns Config/showStartDate. Accepted value is published under State.
    publishRecordingCalendar();
    // Applied schedule values are published by the shared operating service.

    // Make sure Calendar branch exists & is retained on connect
    publishMQTT(MQTT_PUB_DAYOFMONTH,  String(dayOfMonth),  true);
    publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning), true);

    if (countsReady()) {
        // ---- Retained core counts ----
        publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars), true);
        publishMQTT(MQTT_PUB_SHOWTOTAL,  String(totalShowCars),  true);
    }

    publishHourlySnapshot();

    // Count persistence runs per vehicle and retries independently of MQTT.
    start_MqttMillis = millis();
}

void publishTimingConfig() {
    // Commands remain HA-owned; expose the actual applied values separately.
    publishConfigState(MQTT_SUB_TOPIC5, String(carCounterTimeout));
    publishConfigState(MQTT_SUB_TOPIC6, String(waitDuration));
}

bool parseCountCorrection(const char* p,uint32_t& value) {
    if (!*p) return false;
    uint32_t n=0;
    while (*p) {
        if (*p<'0' || *p>'9' || n>(2147483647UL-(*p-'0'))/10) return false;
        n=n*10+(*p++-'0');
    }
    value=n;return true;
}

void callback(char* topic, byte* payload, unsigned int length) {
    // Bound stack use and reject embedded NULs/truncated command payloads.
    if (length>180) return;
    char message[181];memcpy(message,payload,length);message[length]='\0';
    if (strlen(message)!=length) return;
    if (handleOperatingCommand(topic,message)) return;
    String command(topic);
    if (command==String(topic_base_path)+"/Config/showStartDate") {
        acceptShowStartDate(String(message));return;
    }
    // Old retained resets, calendar mirrors and individual timing commands
    // are deliberately inert. RTC owns the date; HA owns the full settings transaction.
}

void applyOperatingDetector(int alarmMs,int detectMs) {
    carCounterTimeout=alarmMs;waitDuration=detectMs;
}
