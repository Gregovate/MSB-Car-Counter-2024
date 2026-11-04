#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.




int computeDaysRunning() {
    if (!showStartDateValid) return 0;
    if (!rtcReady) return 0;

    DateTime nowDT = rtc.now();
    if (nowDT.year() < 2024) return 0;   // sanity guard

    DateTime showStart(showStartY, showStartM, showStartD, 0, 0, 0);

    if (nowDT.unixtime() < showStart.unixtime()) return 0;

    uint32_t diffSeconds = nowDT.unixtime() - showStart.unixtime();
    int diffDays = diffSeconds / 86400UL;

    int dayNum = diffDays + 1;  // opening night = Day 1

    // ---- Preserve historical "no Christmas Eve show day" semantics ----
    // If the season started before Dec 24 (normal MSB case),
    // then subtract 1 for Dec 24 and every day after.
    bool seasonStartsBeforeDec24 =
        (showStartM < 12) || (showStartM == 12 && showStartD <= 23);

    if (seasonStartsBeforeDec24 &&
        nowDT.month() == 12 && nowDT.day() >= 24) {
        dayNum -= 1;
    }

    if (dayNum < 1) dayNum = 1;  // safety clamp once show has started
    return dayNum;
}

int determineSeasonYear() {
    if (showStartDateValid) return showStartY;
    DateTime nowDT = rtc.now();
    return nowDT.year();
}



void timeTriggeredEvents() {
    DateTime now = rtc.now();

    // ---------------------------
    // Midnight reset
    // ---------------------------
    if (now.hour() == 23 && now.minute() == 59 && !flagMidnightReset) { 
        saveHourlyCounts();
        totalDailyCars = 0;
        saveDailyTotal();
        resetHourlyCounts();
        publishMQTT(MQTT_DEBUG_LOG, "Total cars reset at Midnight");
        flagMidnightReset = true;
    }
    
    // =========================================================
    // DaysRunning computed from showStartDate (Christmas Eve gap honored)
    // =========================================================
    if (now.day() != lastDayOfMonth) {

        if (!flagDaysRunningReset) {
            int newDaysRunning = computeDaysRunning();
            if (newDaysRunning > 0 && newDaysRunning != daysRunning) {
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
    if (minutesNow == (showStartMin - 1) && !flagDailyShowStartReset) {
        totalDailyCars = 0;
        saveDailyTotal();
        publishMQTT(MQTT_DEBUG_LOG, "Daily total reset (showStartMin-1)");
        flagDailyShowStartReset = true;
    }

    // =========================================================
    // **MICROSTEP #3 — dynamic show-end summary**
    // Save summary EXACTLY at showEndMin
    // =========================================================
    // Consider the show "active" once a valid showStartDate is set
    bool showSeasonActive =
        showStartDateValid &&
        rtcReady &&
        (now.year() >= showStartY);   // cheap check, you can tighten if needed

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
