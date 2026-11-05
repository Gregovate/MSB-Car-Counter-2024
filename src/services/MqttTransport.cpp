#include "traffic/Firmware.h"

// Shared implementation: keep identical in both projects (verify_modular.py).
String encodeQueuedMessage(const char *topic, const String &msg, bool retainFlag) {
    return String(topic) + "|" + msg + "|" + (retainFlag ? "1" : "0");
}

void publishQueuedMessages(size_t maxToFlush) {
    size_t flushed = 0;

    while (!publishQueue.empty() &&
           mqtt_client.connected() &&
           flushed < maxToFlush) {

        String data = publishQueue.front();
        publishQueue.pop();

        int p1 = data.indexOf('|');
        int p2 = data.indexOf('|', p1 + 1);

        if (p1 != -1) {
            String topic   = data.substring(0, p1);
            String message = data.substring(p1 + 1, p2);
            bool retainFlag = (p2 != -1 && data.substring(p2 + 1) == "1");

            mqtt_client.publish(topic.c_str(), message.c_str(), retainFlag);
        }

        flushed++;
    }

    if (flushed > 0) {
        Serial.printf("MQTT Queue Flush: %u messages flushed, %u remain\n",
                      (unsigned)flushed, (unsigned)publishQueue.size());
    }
}

void publishDebugLog(const String &message) {
    publishMQTT(MQTT_DEBUG_LOG, message);   // never retained
}

void publishDebugEvent(const char* event, const String& details, bool retainFlag) {
    char buf[256];

    snprintf(buf, sizeof(buf),
        "{"
            "\"device\":\"%s\","
            "\"event\":\"%s\","
            "\"fw\":\"%s\","
            "\"time\":\"%s\","
            "\"details\":\"%s\""
        "}",
        THIS_MQTT_CLIENT,
        event,
        FWVersion,                     // <-- NO .c_str()
        bootTimestamp.c_str(),
        details.c_str()
    );

    publishMQTT(MQTT_DEBUG_LOG, String(buf), retainFlag);
}

// HA owns retained Config commands. Report applied values on State only.
// Skip disconnected snapshots: heartbeat republishes current values after
// connection, rather than replaying defaults captured before HA commands.
void publishConfigState(const char* commandTopic, const String& value) {
    if (!mqtt_client.connected()) return;
    String stateTopic(commandTopic);
    if (stateTopic.indexOf("/Config/") < 0) return;
    stateTopic.replace("/Config/", "/State/");
    publishMQTT(stateTopic.c_str(), value, true);
}

// Publish one current 24-hour snapshot for both devices. Never enqueue an
// offline snapshot or report default counts after failed recovery.
void publishHourlySnapshot() {
    if (!countsReady() || !mqtt_client.connected()) return;
    // A complete 24-bin int array plus topic can exceed PubSubClient's default
    // packet capacity. Grow only when needed; do not publish truncated JSON.
    if (mqtt_client.getBufferSize() < 512 && !mqtt_client.setBufferSize(512)) {
        publishDebugLog("Hourly snapshot skipped: MQTT buffer allocation failed");
        return;
    }
    String buckets("[");
    for (int i=0; i<24; ++i) {
        if (i) buckets += ",";
        buckets += String(hourlyCount[i]);
    }
    buckets += "]";
    publishMQTT(MQTT_PUB_HOURLY_JSON, buckets, true);
    for (int i=0; i<24; ++i) {
        char topic[96];
        // MQTT_PUB_CARS_HOURLY already ends with '/'.
        snprintf(topic, sizeof(topic), "%s%02d", MQTT_PUB_CARS_HOURLY, i);
        publishMQTT(topic, String(hourlyCount[i]), true);
    }
}
