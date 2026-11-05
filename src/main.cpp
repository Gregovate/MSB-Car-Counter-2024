/*
 * MSB Car Counter — RELEASE CONTROL
 *
 * Before compiling a changed firmware:
 *   1. Add every change below, newest entry first.
 *   2. Manually update FWVersion immediately below this comment.
 * This is the only version value: MQTT, ElegantOTA and diagnostics use it.
 * Older original history is preserved in docs/Firmware_History.md.
 *
 * CHANGELOG — newest first
 * 25.12.03.2-modular-test.6 — 2026-11-05
 *   - Repeat the latest hourly-export status on heartbeat in both counters.
 *   - Report skipped/failed exports explicitly; do not replay stale count queues.
 *   - Test late status delivery, retained failures and successful recovery.
 * 25.12.03.2-modular-test.5 — 2026-11-05
 *   - Accept a verified zero legacy daily total with older valid hourly rows.
 *   - Preserve historical rows and reject inconsistent nonzero daily totals.
 *   - Add the exact field migration case and post-migration reboot regression.
 * 25.12.03.2-modular-test.4 — 2026-11-04
 *   - Save each vehicle in a shared verified, dated two-copy SD checkpoint.
 *   - Restore counts before reporting; preserve files when recovery is uncertain.
 *   - Retry failed checkpoints independently of MQTT and report persistence faults.
 *   - Use full-date rollover and persist show-reset markers across reboot.
 *   - Verify hourly CSV replacements and retain the previous complete file.
 * 25.12.03.2-modular-test.3 — 2026-11-04
 *   - Share persistent calendar-year selection; never reopen a prior year for writes.
 *   - Protect historical SD folders from logging and web mutations.
 *   - Derive Black Friday fallback annually; persist valid current-year HA dates.
 *   - Report accepted date separately; reject stale dates and compute pre-show day 0.
 *   - Rotate paths while running; retry network clock sync without rolling year back.
 *   - Restore show total on new-day boot before reconnect reporting.
 *   - Preserve GPIOs, detector timing and manual release control.
 * 25.12.03.2-modular-test.2 — 2026-11-04
 *   - Restore the editable firmware identifier and active changelog to main.cpp.
 *   - Share the identifier through FirmwareVersion.h; no duplicate version value.
 *   - Allow deliberate version edits in preservation checks; keep logic checks.
 *   - Make version-helper text access explicitly UTF-8.
 * 25.12.03.2-modular-test.1 — 2026-11-04
 *   - Extract MQTT, network, web/OTA, temperature, clock, SD, display and detector
 *     services; retain existing counting behavior, pins and topic names.
 *   - Preserve original source history and exclude all licensed OTA source.
 *   - Add preservation/shared-file checks and Windows UTF-8/CRLF fixes.
 *   - Record local builds with licensed ElegantOTAPro 3.1.4; no device upload.
 */
#include "traffic/FirmwareVersion.h"
const char FWVersion[] = "25.12.03.2-modular-test.6";

#include "traffic/Firmware.h"

void setup() {
    Serial.begin(115200);
    ElegantOTA.setAutoReboot(true);
    ElegantOTA.setFilesystemMode(true);
    Serial.println("Starting Car Counter...");
    lastCarDetectedMillis = millis(); // Synced initial time at boot  //GAL 25-11-25

    //Initialize Display
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, line1);
    display.println("Initializing...");
    display.display();
  
// Set the ESP32 hostname
    if (WiFi.setHostname(THIS_MQTT_CLIENT)) {
        Serial.printf("Hostname set to: %s\n", THIS_MQTT_CLIENT);
    } else {
        Serial.println("Failed to set hostname!");
    }

    // Scan WiFi networks
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks();
    Serial.println("WiFi scan completed.");
    if (n == 0) {
        Serial.println("No networks found.");
    } else {
        Serial.printf("%d networks found:\n", n);
        for (int i = 0; i < n; ++i) {
            Serial.printf("%d: %s (%d dBm) %s\n",
                          i + 1,
                          WiFi.SSID(i).c_str(),
                          WiFi.RSSI(i),
                          WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SECURE");
        }
    }

    // Add multiple WiFi APs
    configureWifiAccessPoints();
    setup_wifi();

    //If RTC not present, stop and check battery
    if (! rtc.begin()) {
        rtcReady = false;
        Serial.println("Could not find RTC! Check circuit.");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0,line1);
        display.println("Clock DEAD");
        display.display();
        publishMQTT(MQTT_DEBUG_LOG, "ENTER RTC BEGIN FAILED", true);
        } else {
        rtcReady = true;   // GAL 25-11-22
    }

    // Get NTP time from Time Server 
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    initializeRecordingCalendar();
    SetLocalTime();
    updateRecordingCalendar();
    serviceCountState();
    DateTime now = rtc.now();

    // Save boot timestamp
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());

    // If RTC failed earlier, keep placeholder instead of junk time
    if (rtcReady) {
        bootTimestamp = String(buf);
    } else {
        bootTimestamp = "rtc-not-ready";
    }
    
    // MQTT Reconnection with login credentials
    // Connect after SD restoration so boot defaults are never advertised as totals.

    // Reset alarm state on boot / reconnect so HA never sees stale ALARM
    publishMQTT(MQTT_PUB_ALARM, "CLEAR", true);
    carStuckAlarmActive = false;

    // After MQTT connect on Car Counter: publish current beam states as retained
    publishMQTT(MQTT_PUB_BEAM_A_STATE, String(stableFirstBeamState), true);
    publishMQTT(MQTT_PUB_BEAM_B_STATE , String(stableSecondBeamState), true);

    // After initial retained publishes:
    publishTimingConfig();

    //Set Input Pins
    pinMode(firstBeamPin, INPUT_PULLDOWN);
    pinMode(secondBeamPin, INPUT_PULLDOWN);
    pinMode(redArchPin, OUTPUT);
    pinMode(greenArchPin, OUTPUT);
    digitalWrite(redArchPin, HIGH);
    digitalWrite(greenArchPin, HIGH);
    
    // Initialize DHT sensor
    dht.begin();
    Serial.println("DHT22 sensor initialized.");

    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0, line3);
    display.println(" Starting");
    display.println("CarCounter");

    Serial.println  ("Initializing Car Counter");
    Serial.print("Temperature: ");
    Serial.print(tempF);
    Serial.println(" F");
    display.display();
    delay(500); // display Startup
  
    //Initialize SD Card
    // GAL 25-11-18: initSDCard() now calls SD.begin() and will not block on failure
    initSDCard();  // Initialize SD card and ready for Read/Write

    if (sdAvailable) {
        ensureSeasonFolderExists();

        // Ensure /data directory exists (folder, not file)
        if (!SD.exists("/data")) {
            recordingMkdir("/data");
            publishMQTT(MQTT_DEBUG_LOG, "Created /data directory", false);
        }

        // Check and create Required Data files
        checkAndCreateFile(fileName1);
        checkAndCreateFile(fileName2);
        checkAndCreateFile(fileName3);
        checkAndCreateFile(fileName4);
        checkAndCreateFile(fileName6, "DateTime, TimeToPass_ms, EnterDailyTotal, AB_Follow_ms, TimeBetweenCars_ms");
        checkAndCreateFile(fileName7, "Date,DaysRunning,Before5,6PM,7PM,8PM,9PM,ShowTotal,DailyAvgTemp");
        checkAndCreateFile(fileName8);
        checkAndCreateFile(fileName9);
        // NEW: bucketed summary file (per-hour buckets + daily total + running show total)
        checkAndCreateFile(fileName10, "Date,DaysRunning,6PM,7PM,8PM,9PM,DailyTotal,ShowTotal,DailyAvgTemp");

        // Required Hourly Data Files
        createAndInitializeHourlyFile(fileName5);
     
    } else {
        publishMQTT(MQTT_DEBUG_LOG, "SD init failed - skipping SD file setup", false);
    }


    // Initialize Server
    setupServer();

    //on reboot, get totals saved on SD Card
    getSavedValuesOnReboot();  // Update/reset counts based on reboot day
    daysRunning = recordingDaysRunning();
    MQTTreconnect();
    publishRecordingCalendar();

    // ---- Sync retained per-hour MQTT topics to current hourlyCount[] on boot ----
    for (int i = 0; i < 24; i++) {
        char t[80];
        snprintf(t, sizeof(t), "%s%02d", MQTT_PUB_CARS_HOURLY, i);
        publishMQTT(t, String(hourlyCount[i]), true);   // retained
    }

    // Setup MDNS
    if (!MDNS.begin(THIS_MQTT_CLIENT)) {
        Serial.println("Error starting mDNS");
        return;
    }
    delay(1000);
    start_MqttMillis = millis();
}

void loop() {
    updateRecordingCalendar();

    // GAL 25-11-22.4: keepalive on its own clock (not starved by other publishes)
    if (millis() - lastKeepAliveMillis >= (unsigned long)mqttKeepAlive * 1000UL) {
        lastKeepAliveMillis = millis();
        KeepMqttAlive();
    }

    // GAL 25-11-22.4: publish temp/RH JSON every 10 minutes (retained)
    if (millis() - lastTempPubMillis >= TEMP_PUB_INTERVAL_MS) {
        lastTempPubMillis = millis();
        char jsonPayload[100];
        snprintf(jsonPayload, sizeof(jsonPayload),
                "{\"tempF\": %.1f, \"humidity\": %.1f}", tempF, humidity);
        publishMQTT(MQTT_PUB_TEMP, String(jsonPayload), true);
    }

    // Run SD health check every 60 seconds GAL 25-11-23.2
    if (millis() - lastSdCheck > 60000UL) {
        lastSdCheck = millis();
        checkSdHealth();
    }
    DateTime now = rtc.now();

    currentTimeMinute = now.hour()*60 + now.minute(); // convert time to minutes since midnight

    bool dateOk = recordingShowSeasonActive();

    showTime = (dateOk &&
                currentTimeMinute >= showStartMin &&
                currentTimeMinute <= showEndMin);

    readTempandRH();          // Get Temperature and Humidity

    ElegantOTA.loop();        // Keep OTA Updates Alive

    updateDisplay();          // Update the display

    checkWiFiConnection();    // Check and maintain WiFi connection

    static unsigned long lastPreferCheck = 0;
    if (millis() - lastPreferCheck > 60000UL) {   // check once per minute
        lastPreferCheck = millis();
        preferPrimaryIfAvailable();
    }

    timeTriggeredEvents();    // Various functions/saves/resets based on time of day

    detectCar();              // Detect cars

    // ----- IDLE PATTERN: run if no car for 30s -----
    if (showTime && (millis() - noCarTimer) >= 30000UL) {
        playPattern();
    }

    // //Added to kepp mqtt connection alive and periodically publish select values
    // if ((millis() - start_MqttMillis) > (mqttKeepAlive * 1000)) {
    //     KeepMqttAlive();
    // }

}

