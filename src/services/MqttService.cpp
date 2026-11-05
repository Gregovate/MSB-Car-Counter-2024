#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.


void publishMQTT(const char *topic, const String &message, bool retainFlag) {
    if (!countTopicPublishable(topic)) return;
    if (mqtt_client.connected()) {
        mqtt_client.publish(topic, message.c_str(), retainFlag);
    } else {
        if (publishQueue.size() >= MAX_QUEUE) {
            publishQueue.pop();    // drop oldest
        }
        Serial.printf("MQTT offline. QUEUEING: %s -> %s (retain=%d)\n",
                      topic, message.c_str(), retainFlag);

        publishQueue.push(encodeQueuedMessage(topic, message, retainFlag));
    }
    start_MqttMillis = millis();
}

void publishMQTT(const char *topic, const String &message) {
    publishMQTT(topic, message, false);
}







void KeepMqttAlive() {
    publishTimingConfig();
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
    publishMQTT(MQTT_PUB_SHOW_START_MIN,  String(showStartMin), true);
    publishMQTT(MQTT_PUB_SHOW_END_MIN,    String(showEndMin), true);

    // Make sure Calendar branch exists & is retained on connect
    publishMQTT(MQTT_PUB_DAYOFMONTH,  String(dayOfMonth),  true);
    publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning), true);

    if (totalsInitialized) {
        // ---- Retained core counts ----
        publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars), true);
        publishMQTT(MQTT_PUB_SHOWTOTAL,  String(totalShowCars),  true);
    }

    // ---- Hourly Snapshot (retained, unchanged) ----
    char buckets[180];
    snprintf(buckets, sizeof(buckets),
        "[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
        hourlyCount[0], hourlyCount[1], hourlyCount[2], hourlyCount[3],
        hourlyCount[4], hourlyCount[5], hourlyCount[6], hourlyCount[7],
        hourlyCount[8], hourlyCount[9], hourlyCount[10], hourlyCount[11],
        hourlyCount[12], hourlyCount[13], hourlyCount[14], hourlyCount[15],
        hourlyCount[16], hourlyCount[17], hourlyCount[18], hourlyCount[19],
        hourlyCount[20], hourlyCount[21], hourlyCount[22], hourlyCount[23]
    );
    publishMQTT(MQTT_PUB_HOURLY_JSON, String(buckets), true);

    // Count persistence runs per vehicle and retries independently of MQTT.
    start_MqttMillis = millis();
}

void publishTimingConfig() {
    // Commands remain HA-owned; expose the actual applied values separately.
    publishConfigState(MQTT_SUB_TOPIC5, String(carCounterTimeout));
    publishConfigState(MQTT_SUB_TOPIC6, String(waitDuration));
}

void callback(char* topic, byte* payload, unsigned int length) {

    // Copy payload into a safe, null-terminated buffer
    char message[length + 1];
    strncpy(message, (char*)payload, length);
    message[length] = '\0';

    if (strcmp(topic, MQTT_SUB_TOPIC1) == 0)  {
        /* Topic used to manually reset Enter Daily Cars */
        totalDailyCars = atoi(message);
        saveDailyTotal();
        Serial.println(F(" Car Counter Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Daily Total Updated");

    } else if (strcmp(topic, MQTT_SUB_TOPIC2) == 0)  {
        /* Topic used to manually reset Total Show Cars */
        totalShowCars = atoi(message);
        saveShowTotal();
        Serial.println(F(" Show Counter Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Show Counter Updated");

    } else if (strcmp(topic, MQTT_SUB_TOPIC3) == 0)  {
        /* Topic used to manually reset Calendar Day */
        dayOfMonth = atoi(message);
        saveDayOfMonth();
        Serial.println(F(" Calendar Day of Month Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Calendar Day Updated");

    } else if (strcmp(topic, MQTT_SUB_TOPIC4) == 0)  {
        /* Topic used to manually reset Days Running */
        daysRunning = atoi(message);
        saveDaysRunning();
        Serial.println(F(" Days Running Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Days Running Updated");

    } else if (strcmp(topic, MQTT_SUB_TOPIC5) == 0)  {
        // Topic used to change car counter timeout (ms)
        unsigned long v = strtoul(message, nullptr, 10);
        if (v < 5000) v = 5000;        // floor 5s
        if (v > 600000) v = 600000;    // ceiling 10 min
        carCounterTimeout = v;

        Serial.println(F(" Counter Alarm Timer Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Car Counter Timeout Updated");

        // Acknowledge the applied value without replacing the HA command.
        publishTimingConfig();

    // *** DISABLE THIS FOR NOW ***
    /*
    } else if (strcmp(topic, MQTT_SUB_TOPIC6) == 0)  {
        // Topic used to change beam stuck-high duration (ms)
        unsigned long v = strtoul(message, nullptr, 10);
        if (v < 5000) v = 5000;
        if (v > 600000) v = 600000;
        waitDuration = v;

        Serial.println(F(" Beam Sensor Duration Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Beam Sensor Duration Updated");
    */
    } else if (strcmp(topic, MQTT_SUB_SHOWSTART) == 0) {
        // Date commands cannot change the recording folder or reset counts.
        acceptShowStartDate(String(message));

    } else if (strcmp(topic, MQTT_SUB_SHOWSTARTTIME) == 0) {
        // Update show start time (minutes since midnight)
        // Expect hh:mm
        int hh = 0, mm = 0;
        if (sscanf(message, "%d:%d", &hh, &mm) == 2) {
            showStartMin = hh * 60 + mm;

            Serial.printf("Show Start Time Updated → %02d:%02d (%d minutes)\n",
                          hh, mm, showStartMin);

            publishMQTT(MQTT_DEBUG_LOG,
                        String("Show Start Time Updated to ") + message);
        } else {
            publishMQTT(MQTT_DEBUG_LOG,
                        String("Invalid ShowStartTime payload: ") + message);
        }

    } else if (strcmp(topic, MQTT_SUB_SHOWENDTIME) == 0) {
        // Update show end time (minutes since midnight)
        // Expect hh:mm
        int hh = 0, mm = 0;
        if (sscanf(message, "%d:%d", &hh, &mm) == 2) {
            showEndMin = hh * 60 + mm;

            Serial.printf("Show End Time Updated → %02d:%02d (%d minutes)\n",
                          hh, mm, showEndMin);

            publishMQTT(MQTT_DEBUG_LOG,
                        String("Show End Time Updated to ") + message);
        } else {
            publishMQTT(MQTT_DEBUG_LOG,
                        String("Invalid ShowEndTime payload: ") + message);
        }
    }
}

