#include "traffic/Firmware.h"

// Shared implementation: keep identical in both projects (verify_modular.py).
String getRtcTimestamp() {
    if (!rtcReady) return "rtc-not-ready";  // GAL 25-11-22

    DateTime now = rtc.now();

    char buf[32];
    snprintf(buf, sizeof(buf),
             "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());

    return String(buf);
}

void SetLocalTime() {
    // Wi-Fi alone is not proof of time. Keep the RTC unless the network clock
    // supplies a valid date at or above the persisted recording year.
    if (WiFi.status() != WL_CONNECTED) return;
    struct tm info;
    if (!getLocalTime(&info, 10)) return;
    int year=info.tm_year+1900;
    if (!traffic::validDate(year,info.tm_mon+1,info.tm_mday) || year<recordingYear()) return;
    noteNetworkClock(year);
    if (rtcReady) rtc.adjust(DateTime(year,info.tm_mon+1,info.tm_mday,
                                     info.tm_hour,info.tm_min,info.tm_sec));
}

void resetHourlyCounts() {
    for (int i = 0; i < 24; i++) {
        hourlyCount[i] = 0; // Reset the hourly counts
    }
    // Log the reset
    Serial.println("Hourly counts have been reset.");
    publishMQTT(MQTT_DEBUG_LOG, "Hourly counts reset for the new day.");
}

