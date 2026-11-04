#include "traffic/Firmware.h"

// Shared implementation: keep identical in both projects (verify_modular.py).
void readTempandRH() {
    // --- Non-blocking warmup and timing state (function-local statics) ---
    static bool dhtWarmupDone = false;
    static unsigned long dhtWarmupStart = 0;

    static unsigned long lastDHTReadMillis  = 0;    // Last time temperature was read
    static unsigned long lastDHTPrintMillis = 0;    // Last time temperature was printed

    const unsigned long DHT_WARMUP_MS      = 10000;   // 10 seconds warmup
    const unsigned long dhtReadInterval    = 60000;   // 60 seconds interval for reading temp
    const unsigned long dhtPrintInterval   = 600000;  // 10 minutes interval for printing temp

    static bool tempOutOfRangeReported = false;

    unsigned long currentMillis = millis();

    // --- Warmup gate: don't touch tempF/humidity until sensor has stabilized ---
    if (!dhtWarmupDone) {
        if (dhtWarmupStart == 0) {
            dhtWarmupStart = currentMillis;  // start warmup on first call
        }
        if (currentMillis - dhtWarmupStart < DHT_WARMUP_MS) {
            return;  // too early, skip DHT entirely
        }
        dhtWarmupDone = true;  // from now on, reads are allowed
    }

    // --- Interval gate: only read every dhtReadInterval ms ---
    if (currentMillis - lastDHTReadMillis < dhtReadInterval) {
        return;
    }
    lastDHTReadMillis = currentMillis;

    // Read temperature and humidity
    humidity = dht.readHumidity();
    tempF   = dht.readTemperature(true); // Read Fahrenheit directly

    // Check if the readings are valid
    if (isnan(tempF) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        publishDebugLog("DHT sensor reading failed.");
        tempF   = -999;  // sentinel
        humidity = -999;
        return; // Exit function if the readings are invalid
    }

    // Check for temperature out of range
    if (tempF < -40 || tempF > 120) {
        if (!tempOutOfRangeReported) {
            Serial.println("Temperature out of range!");
            publishDebugLog("DHT temperature out of range: " + String(tempF));
            tempOutOfRangeReported = true; // prevent duplicate reporting
        }
        tempF = -999; // sentinel for out-of-range condition
    } else {
        // Reset the flag if temperature is back in range
        if (tempOutOfRangeReported) {
            Serial.println("Temperature back in range.");
            tempOutOfRangeReported = false;
        }

        // Build JSON for temp/RH (actual MQTT publish happens elsewhere on your 10-min timer)
        char jsonPayload[100];
        snprintf(jsonPayload, sizeof(jsonPayload),
                 "{\"tempF\": %.1f, \"humidity\": %.1f}", tempF, humidity);
        // GAL 25-11-22.4: temp/RH publish moved to 10-min timer in loop to reduce spam
        // publishMQTT(MQTT_PUB_TEMP, String(jsonPayload));

        // Forward valid readings to the hourly average system
        averageHourlyTemp(); // Ensure the reading is processed for summaries
    }

    // --- 10-minute printout for serial diagnostics ---
    if (currentMillis - lastDHTPrintMillis >= dhtPrintInterval) {
        lastDHTPrintMillis = currentMillis;

        if (tempF != -999 && humidity != -999) {
            Serial.printf("Temperature: %.1f °F, Humidity: %.1f %%\n", tempF, humidity);
        } else {
            Serial.println("Temperature/Humidity data invalid. Check sensor.");
        }
    }
}
