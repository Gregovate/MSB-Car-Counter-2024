#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.




int computeDaysRunning() {
    return recordingDaysRunning();
}

int determineSeasonYear() {
    return recordingYear();
}



void timeTriggeredEvents() {
    if (!rtcReady || rtc.lostPower() || rtc.now().year() != recordingYear()) return;
    DateTime now = rtc.now();

    // Full-date rollover is serviced before detection, not at 23:59.
    if (!countsReady()) return;

    // =========================================================
    // DaysRunning computed from showStartDate (Christmas Eve gap honored)
    // =========================================================
    if (now.day() != lastDayOfMonth) {

        if (!flagDaysRunningReset) {
            int newDaysRunning = computeDaysRunning();
            if (newDaysRunning != daysRunning) {
                daysRunning = newDaysRunning;
                saveDaysRunning();
                publishMQTT(MQTT_DEBUG_LOG,
                            "DaysRunning (computed): " + String(daysRunning));
            }
        }

        if (!flagDaysRunningReset) {
            dayOfMonth = now.day();
            saveDayOfMonth();
            getDayOfMonth();
            publishMQTT(MQTT_DEBUG_LOG, "Day of month: " + String(dayOfMonth));
            lastDayOfMonth = now.day();
            flagDaysRunningReset = true;
        }
    }

    // =========================================================
    // **MICROSTEP #2 — dynamic show-start reset**
    // Reset 1 minute before showStartMin
    // =========================================================
    int minutesNow = now.hour() * 60 + now.minute();
    if (recordingShowSeasonActive() && minutesNow == (showStartMin - 1) && !flagDailyShowStartReset) {
        if (resetCountsForShow())
            publishMQTT(MQTT_DEBUG_LOG, "Show-window daily reset checkpointed");
    }

    // =========================================================
    // **MICROSTEP #3 — dynamic show-end summary**
    // Save summary EXACTLY at showEndMin
    // =========================================================
    // Apply the shared current-year calendar and Christmas Eve exclusion.
    bool showSeasonActive = recordingShowSeasonActive();

    // Primary trigger: normal end-of-show save
    if (sdAvailable &&
        showSeasonActive &&
        (minutesNow == showEndMin) &&
        !flagDailyShowSummarySaved) {

        saveDailyShowSummary();
        publishMQTT(MQTT_DEBUG_LOG, "Daily Show Summary Saved (showEndMin)");
        flagDailyShowSummarySaved = true;
    }

    // // Fail-safe: if we missed the exact minute, still write ONE summary row later
    // // GAL 25-12-02: disabled, causing duplicate ShowSummary rows note of reboots after written
    //     showSeasonActive &&
    //     (minutesNow > showEndMin) &&
    //     !flagDailyShowSummarySaved &&
    //     now.hour() < 23) {

    //     saveDailyShowSummary();
    //     publishMQTT(MQTT_DEBUG_LOG,
    //                 "Daily Show Summary Saved (failsafe after showEndMin)");
    //     flagDailyShowSummarySaved = true;
    // }

    // ---------------------------
    // Reset daily flags at 12:01:01 AM
    // ---------------------------
    if (now.hour() == 0 && now.minute() == 1 && now.second() == 1 && !resetFlagsOnce) { 
        flagDaysRunningReset = false;
        flagMidnightReset = false;
        flagDailyShowStartReset = false;
        flagDailySummarySaved = false;
        flagDailyShowSummarySaved = false;
        flagHourlyReset = false;
        publishMQTT(MQTT_DEBUG_LOG, "Run once flags reset for new day");
        resetFlagsOnce = true;
    }

    // Allow next day’s reset
    if (now.hour() == 0 && now.minute() == 2 && now.second() == 0) {
        resetFlagsOnce = false;
    }
}

