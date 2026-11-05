#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.
void countTheCar() {
    /* COUNT SUCCESS */

    noCarTimer = millis();

    // RTC snapshot
    DateTime now = rtc.now();

    // Build a fresh timestamp string (do NOT use now.toString here)
    char timeBuf[25];
    snprintf(timeBuf, sizeof(timeBuf),
             "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());

    // Serial log (no temp/humidity here either if you don't care)
    Serial.print(timeBuf);
    Serial.print(", EnterDailyTotal (before increment) = ");
    Serial.println(totalDailyCars);

    // Increase count for every car going through car counter regardless of time
    totalDailyCars++;

    // Increment hourly car count
    int currentHour = now.hour();
    hourlyCount[currentHour]++;
    pendingSaveHourly = true;   // defer SD hourly write
    pendingSaveDaily  = true;   // defer daily total write

    // Construct the MQTT topic dynamically
    char topic[60];
    snprintf(topic, sizeof(topic), "%s%02d", MQTT_PUB_CARS_HOURLY, currentHour);
    publishMQTT(topic, String(hourlyCount[currentHour]));

    // Increase Show Count only when show is open
    if (showTime == true) {
        totalShowCars++;
        pendingSaveShow = true;  // defer show total write
    }

    Serial.print(F("TotalDailyCars after increment: "));
    Serial.println(totalDailyCars);

    // Build full path: /CC/2025/EnterLog.csv
    String enterLogPath = String(seasonFolder) + fileName6;

    // Open file for writing Car Data
    myFile = recordingOpen(enterLogPath.c_str(), FILE_APPEND); // EnterLog.csv in season folder
    if (myFile) {
        // NEW CSV FORMAT:
        // DateTime, TimeToPass_ms, EnterDailyTotal, AB_Follow_ms, TimeBetweenCars_ms

        myFile.print(timeBuf);
        myFile.print(", ");
        myFile.print(timeToPassMS);        // A->B broken -> B clear (set in CAR_DETECTED)
        myFile.print(", ");
        myFile.print(totalDailyCars);      // EnterDailyTotal
        myFile.print(", ");
        myFile.print(abFollow_ms);         // A->B follow time (Beam 2 first high)
        myFile.print(", ");
        myFile.println(timeBetweenCars_ms); // raw millis between cars

        myFile.close();

        Serial.print(F(" Car "));
        Serial.print(totalDailyCars);
        Serial.println(F(" Logged to SD Card."));

        // Only car-related MQTT here; env is handled by 10-minute task
        publishMQTT(MQTT_PUB_TIME, timeBuf);
        publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars));
        start_MqttMillis = millis();

    } else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fileName6);
    }
}

void detectCar() {
    unsigned long currentMillis = millis();
    bool rawFirstBeamState  = digitalRead(firstBeamPin);
    bool rawSecondBeamState = digitalRead(secondBeamPin);

    // ---- Debounce first beam sensor ----
    if (rawFirstBeamState != stableFirstBeamState) {
        if (currentMillis - lastFirstBeamChangeTime > debounceDelay) {
            stableFirstBeamState = rawFirstBeamState;
            lastFirstBeamChangeTime = currentMillis;
            // Publish the state change only when it's stable
            publishMQTT(MQTT_PUB_BEAM_A_STATE  , String(stableFirstBeamState), true);
        }
    } else {
        lastFirstBeamChangeTime = currentMillis;
    }

    // ---- Debounce second beam sensor ----
    if (rawSecondBeamState != stableSecondBeamState) {
        if (currentMillis - lastSecondBeamChangeTime > debounceDelay) {
            stableSecondBeamState = rawSecondBeamState;
            lastSecondBeamChangeTime = currentMillis;
            // Publish the state change only when it's stable
            publishMQTT(MQTT_PUB_BEAM_B_STATE, String(stableSecondBeamState), true);
        }
    } else {
        lastSecondBeamChangeTime = currentMillis;
    }

    // Use the stable beam states instead of the raw readings
    firstBeamState  = stableFirstBeamState;
    secondBeamState = stableSecondBeamState;

    // ---- State machine Car Counter BEGIN ----
    switch (currentCarDetectState) {

        case WAITING_FOR_CAR: {
            unsigned long now = currentMillis;

            // Beam 1 health
            if (firstBeamState == 1) {
                if (firstBeamHealth_ms == 0) {
                    firstBeamHealth_ms = now;  // start timing
                } else if ((now - firstBeamHealth_ms) >= carCounterTimeout && !carStuckAlarmActive) {
                    publishMQTT(MQTT_COUNTER_LOG, "Beam 1 stuck HIGH (idle)");
                    publishMQTT(MQTT_PUB_ALARM, "ALARM_ENTER_STUCK", true);
                    carStuckAlarmActive = true;
                }
            } else {
                // Beam 1 is clear
                firstBeamHealth_ms = 0;
                // *** NO CLEAR HERE ANYMORE ***
            }

            // Beam 2 health
            if (secondBeamState == 1) {
                if (secondBeamHealth_ms == 0) {
                    secondBeamHealth_ms = now;  // start timing
                } else if ((now - secondBeamHealth_ms) >= carCounterTimeout && !carStuckAlarmActive) {
                    publishMQTT(MQTT_COUNTER_LOG, "Beam 2 stuck HIGH (idle)");
                    publishMQTT(MQTT_PUB_ALARM, "ALARM_ENTER_STUCK", true);
                    carStuckAlarmActive = true;
                }
            } else {
                // Beam 2 is clear
                secondBeamHealth_ms = 0;
                // *** NO CLEAR HERE ANYMORE ***
            }

            // *** NEW: single CLEAR gate ***
            if (firstBeamState == 0 && secondBeamState == 0 && carStuckAlarmActive) {
                publishMQTT(MQTT_PUB_ALARM, "CLEAR", true);
                carStuckAlarmActive = false;
            }

            // Normal entry into car detection
            if (firstBeamState == 1) {
                firstBeamTripTime      = currentMillis;
                abFollowCaptured       = false;            // reset GLOBAL flag
                currentCarDetectState  = FIRST_BEAM_HIGH;
                publishMQTT(MQTT_COUNTER_LOG, "First Beam High");
                digitalWrite(greenArchPin, HIGH);         // Green arch ON
            }
            break;
        }

        case FIRST_BEAM_HIGH:
            if (secondBeamState == 1) {

                // First time Beam B follows Beam A → capture A→B follow ONCE
                if (!abFollowCaptured) {
                    abFollow_ms = currentMillis - firstBeamTripTime;
                    publishMQTT(MQTT_PUB_BEAM_AB_MS, String(abFollow_ms));
                    abFollowCaptured = true;
                }

                // Use current time for activation thresholds
                unsigned long timeBeamsHighNow = currentMillis - firstBeamTripTime;

                // Only move to BOTH_BEAMS_HIGH if duration exceeds activation thresholds
                if (timeBeamsHighNow >= minActivationDuration &&
                    timeBeamsHighNow >= waitDuration) {
                    bothBeamsHighTime     = currentMillis;
                    carPresentFlag        = true;
                    currentCarDetectState = BOTH_BEAMS_HIGH;
                    publishMQTT(MQTT_COUNTER_LOG, "State Changed: Both Beams High");
                    digitalWrite(greenArchPin, LOW);   // Green arch OFF
                    digitalWrite(redArchPin, HIGH);    // Red arch ON
                }

            } else if (firstBeamState == 0) {
                // First beam cleared before second beam ever went high -> reset
                currentCarDetectState = WAITING_FOR_CAR;
                publishMQTT(MQTT_COUNTER_LOG, "1st Beam Low, Changed Waiting for Car");
                digitalWrite(greenArchPin, HIGH);      // Green arch ON
                digitalWrite(redArchPin, LOW);         // Red arch OFF

            } else {
                // firstBeamState == 1 and secondBeamState == 0 → Beam 1 has been high alone
                if ((currentMillis - firstBeamTripTime) >= carCounterTimeout && !carStuckAlarmActive) {
                    publishMQTT(MQTT_COUNTER_LOG, "Beam 1 stuck HIGH (FIRST_BEAM_HIGH)");
                    publishMQTT(MQTT_PUB_ALARM, "ALARM_ENTER_STUCK", true);
                    carStuckAlarmActive = true;
                }
            }
            break;

        case BOTH_BEAMS_HIGH:
            // Stuck-vehicle alarm using carCounterTimeout
            if ((currentMillis - firstBeamTripTime) >= carCounterTimeout) {
                if (!carStuckAlarmActive) {
                    publishMQTT(MQTT_COUNTER_LOG, "Sensor blocked", true);
                    publishMQTT(MQTT_PUB_ALARM, "ALARM_ENTER_STUCK", true);
                    carStuckAlarmActive = true;   // latch so we don't spam
                }
            }

            // Beam B cleared and we think a car is present -> end of event
            if (secondBeamState == 0 && carPresentFlag) {
                // Measure how long both beams / Beam B were high
                unsigned long brokenDuration = currentMillis - bothBeamsHighTime;

                publishMQTT(
                    MQTT_COUNTER_LOG,
                    "Beam B clear. Broken duration: " + String(brokenDuration) + " ms"
                );
                publishMQTT(MQTT_PUB_BEAM_B_BROKEN_MS , String(brokenDuration));

                // IMPORTANT: Always treat this as a real car if we got this far,
                // just like the 2024 version. No gating on brokenDuration.
                currentCarDetectState = CAR_DETECTED;
                publishMQTT(MQTT_COUNTER_LOG, "Changed state to Car Detected", false);

                // Clear alarm when the event ends (Beam B clear)
                if (carStuckAlarmActive) {
                    publishMQTT(MQTT_PUB_ALARM, "CLEAR", true);
                    carStuckAlarmActive = false;
                }
            }
            break;

        case CAR_DETECTED:
            if (carPresentFlag) {
                // Compute full pass time into GLOBAL timeToPassMS for logging
                timeToPassMS = currentMillis - firstBeamTripTime;

                // Count the car and update files (countTheCar() will log timeToPassMS)
                countTheCar();
                publishMQTT(MQTT_PUB_TTP, String(timeToPassMS));
                publishMQTT(MQTT_COUNTER_LOG, "Car Counted Successfully!!");
                carPresentFlag = false;

                // Lights on successful detection
                digitalWrite(redArchPin, LOW);    // Red OFF
                digitalWrite(greenArchPin, HIGH); // Green ON

                // Time between cars
                timeBetweenCars_ms = currentMillis - lastCarDetectedMillis;
                publishMQTT(MQTT_PUB_BETWEENCARS_MS, String(timeBetweenCars_ms), true);
                lastCarDetectedMillis = currentMillis;

                // Back to idle
                currentCarDetectState = WAITING_FOR_CAR;
                publishMQTT(MQTT_COUNTER_LOG, "Idle-Waiting For Car");
            }
            break;

    }   // ---- State machine Car Counter END ----
}

