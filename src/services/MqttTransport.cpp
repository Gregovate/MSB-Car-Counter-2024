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
