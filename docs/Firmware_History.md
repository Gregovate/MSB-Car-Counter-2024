# Preserved firmware source history

Verbatim introductory comments from the pre-extraction main.cpp. Current interfaces live under include/traffic.

```text
/*
Car counter Interface to MQTT by Greg Liebig greg@engrinnovations.com
Initial Build 12/5/2023 12:15 pm
Stores data on SD card in event of internet failure
DOIT DevKit V1 ESP32 with built-in WiFi & Bluetooth */

// IMPORTANT: Update FWVersion each time a new changelog entry is added
#define OTA_Title "Car Counter" // OTA Title
#define FWVersion "25.12.03.2"
#define THIS_MQTT_CLIENT "espCarCounter" // MQTT Client Name

/* ## CAR COUNTER BEGIN CHANGELOG ##
25.12.03.2   Updated CarCounter web UI and summary views:
             - Added new showSummaryBuckets.html viewer for bucket-style
               summary data and linked it into the main index.html page.
             - Added HTTP routes /showSummaryBuckets.html and
               /ShowSummaryBuckets.csv to serve the new HTML viewer and
               the derived bucket CSV file.
             - Updated index.html to match GateCounter layout and added
               dual summary buttons (cumulative + bucket).
             - Updated CarCounter listSDFiles() output to use tab-aligned
               formatting identical to GateCounter for improved readability.
             - No changes to detection logic or MQTT topics.
25.12.03.1   Added second daily summary file (ShowSummaryBuckets.csv) to CarCounter:
             - Introduced new filename10 constant and automatic header creation for
               the bucket-style summary CSV in /CC/YYYY/.
             - saveDailyShowSummary() now writes both cumulative summary
               (ShowSummary.csv) and new hourly bucket summary
               (6PM,7PM,8PM,9PM plus DailyTotal).
             - Bucket math derived from existing cumulative values; does not alter
               legacy ShowSummary.csv format or behavior.
             - Ensures backward compatibility with committee spreadsheets while
               providing incremental per-hour data for improved nightly reporting.
25.12.03.0  Replaced Beam1_Trip_ms with TimeBetweenCars_ms in EnterLog.csv to
             log actual time between cars instead of redundant Beam-1 trip time.
             Updated all related comments, variable names, and CSV headers.
25.12.02.2  Added dates for file listings in SD File Manager UI
25.12.02.1  Disabled Failsafe to avoid writing duplicate summary rows 25.12.02.1-MQTTfix
25.12.02.0  25.12.02.0   Added full beam-health protection and unified alarm handling.
             - Added Beam-1 idle fault detection in FIRST_BEAM_HIGH:
               Raises ALARM_ENTER_STUCK if Beam-1 stays HIGH and Beam-2
               never trips for >= carCounterTimeout.
             - Added Beam-1 and Beam-2 idle health timers in WAITING_FOR_CAR.
             - Replaced per-beam CLEAR logic with single gate:
               Alarm clears only when BOTH beams are LOW and system is idle.
             - All ALARM and CLEAR publishes are now retained.
             - Added boot-time retained CLEAR publish after MQTT reconnect
               to prevent stale ALARM states in Home Assistant.
             - No changes to core car detection timing, A→B follow logic,
               pass timing, or counting behavior.
25.12.01.0   Added missing /delete route in setupServer() to fix SD file delete
             failures (HTTP 500). Added new /rename route and renameSDFile()
             handler for in-place file renames (e.g., *.csv → *.bak) without
             pulling the SD card.
             Updated file manager UI:
             - index.html now uses /delete via GET and adds a Rename File UI
               tied to /rename.
             - All styling moved to /style.css (no inline <style>).
             - New /identity endpoint returns espCarCounter and drives
               identity-based theming (Car vs Gate) via body classes
               theme-car / theme-gate.
             - “Delete File” button is red and gated by a confirm dialog.
25.11.30.1   Corrected A→B follow detection logic. Removed duplicate/multi-publish
             of beamAB_ms and replaced with single-capture abFollow_ms using
             global flag abFollowCaptured. Updated CAR_DETECTED to store the full
             pass time in global timeToPassMS instead of a local shadow variable.
             EnterLog.csv format updated to:
             "DateTime, TimeToPass_ms, EnterDailyTotal, AB_Follow_ms,
             Beam1_Trip_ms". Removed tempF/humidity from car-logging path.
25.11.30.0   Replaced rtc.toString(buf2) in countTheCar() with explicit timestamp
             formatting using snprintf() to prevent format-buffer mutation. Fixes
             frozen timestamps in EnterLog.csv. No other functional changes.
25.11.29.4  Fixed problem with files needing a header it wasn't creating a new line
25.11.29.3   ShowSummary Fail-Safe Append Logic (2025 Season Only)
             - Hardened daily show summary logic so ShowSummary.csv always gets one row per
               show night when the SD card is available.
             - Primary trigger remains showEndMin; added a single fail-safe write if showEndMin
               is missed but the system is still running later that evening.
             - Prevents empty ShowSummary.csv for valid show nights without touching any 2024
               data or existing rows in the 2025 file.
25.11.29.2   SD File Manager Path Normalization and Browser Download/Upload Fixes
             - Corrected all SD file operations to use a unified buildPath() helper for directory-safe
               path construction across browse, upload, download, delete, and directory navigation.
             - Fixed missing slash issues (e.g., /cc/2025ShowSummary.csv) that caused false "File not found"
               errors when accessing seasonal log files in /CC/YYYY.
             - Updated downloadSDFile(), uploadSDFile(), deleteSDFile(), and changeDirectory() to ensure
               consistent path handling, normalized leading slashes, and robust directory traversal.
             - Restored full SD File Manager usability: listing, navigating, downloading, and uploading
               files within seasonal folders works reliably again.
             - No changes to beam logic, MQTT reporting, or SD core storage routines in this build.

25.11.29.1   SD-Offline Hardening and MQTT Safety Fix
             - Removed all unintended MQTT publishes in the no-SD branch of getSavedValuesOnReboot().
             - Eliminated saveDayOfMonth() and saveDaysRunning() calls when SD is unavailable.
               > This prevented DayOfMonth and DaysRunning from being reset in HA during SD failure.
             - Added totalsInitialized flag to block KeepMqttAlive() and MQTTreconnect() from pushing
               zero or uninitialized totals to HA.
               > HA now retains last known totals when SD is offline instead of being overwritten.
             - SD-available behavior unchanged: totals reload correctly from SD and publish normally.
             - This closes the issue where any reboot with SD offline caused HA totals to reset to zero.
             - Lays groundwork for future enhancement: restore from HA retained totals when SD unavailable.

25.11.29.0   Restored 2024 car-detection behavior: removed Beam B "short duration" rejection.
            > Any valid A->B event that reaches BOTH_BEAMS_HIGH and clears B now counts a car.
            - Added Enter A->B timing metric (beamAB_ms) published to:
                 msb/traffic/CarCounter/Sensors/beamAB_ms
            - No changes to MQTT topic names or SD/RTC logic in this build.
            Aligned MQTT Definitions to the gate counter for debugging sanity.
25.11.28.3  Increased DHT sensor read interval from 10 seconds to 60 seconds to 
            reduce sensor polling frequency and added a warm-up period on boot. Took out
            waitduration from Car Detection callback as it was not being used.
25.11.28.2  Micro-change: remove the idempotent guard so any valid ShowStartDate always recomputes daysRunning.
25.11.28.1  Removed feedback for Beamsensor duration and Car Counter Timeout updates
            to reduce MQTT spam during HA retained publishes.
25.11.28.0  - Updated System/hello to publish JSON payload:
                {"device":"espCarCounter","status":"online","fw":"<ver>","boot":"<ts>","msg":"Car Counter ONLINE"}
            This aligns structure with Gate Counter and fixes HA status sensor.
            - Standardized stuck/alarm human-readable message in BOTH_BEAMS_HIGH state:
                publishMQTT(MQTT_COUNTER_LOG, "Sensor blocked", false);
            Replaces old text "Alarm: Car stuck at car counter" and removed unused legacy HELLO debug line.
25.11.27.0  Fixed Car Counter config topics for alarm and beam wait tuning:
            - Corrected MQTT_SUB_TOPIC5 to /Config/carCounterTimeout (ms) for stuck-car alarm.
            - Corrected MQTT_SUB_TOPIC6 to /Config/beamWaitDuration (ms) for second-beam wait.
            - No behavioral changes; timers still stored internally in milliseconds.

25.11.26.1  Hardened SD/MQTT interaction for Car Counter:
            - Decoupled Calendar and totals MQTT publishes from SD availability
                (saveDailyTotal, saveShowTotal, saveDayOfMonth, saveDaysRunning,
                saveHourlyCounts); MQTT now always reflects in-RAM counters.
            - Reworked getSavedValuesOnReboot() to recompute DaysRunning from RTC
                + showStartDate and keep Calendar/dayOfMonth and daysRunning correct
                even when SD is missing or unreadable; SD loaders (getDailyTotal,
                getShowTotal, getDayOfMonth, getDaysRunning, getHourlyData) no
                longer wipe values on failure.
            - Removed MQTT_PUB_SUMMARY from saveDailyShowSummary(); daily summary
                is now SD-only for manual spreadsheets instead of polluting Calendar.
            - Added periodic checkSdHealth() monitor and improved SD diagnostics so
                SD failures degrade gracefully without breaking dashboards; publish
                retained Calendar/dayOfMonth and Calendar/daysRunning on MQTT
                reconnect so HA never sees UNKNOWN after reboot.
            Hardened SD/MQTT behavior (continued improvements):
                - Moved SD status and diagnostics to new MQTT topic tree:
                      /System/sd/status
                      /System/sd/diag
                      /System/sd/health
                - Updated publishSdDiag_() to publish JSON directly (no extra String())
                  and aligned all SD diagnostics with new retained topics.
                - Added retained SD health reporting in initSDCard() paths
                  (OK, OFFLINE, DEGRADED) for HA stability after reboots.
                - Integrated new MQTT_PUB_SD_STATUS, MQTT_PUB_SD_DIAG,
                  and MQTT_PUB_SD_HEALTH across initSDCard(), SD errors,
                  and runtime health checks.
25.11.26.0  Major MQTT topic cleanup + timing diagnostics
            - Normalized all MQTT topics under new 2025 tree:
                /System /Env /Cars /Calendar /Sensors /Config
            - Moved all reset topics into /Config hierarchy
            - Standardized WiFi diagnostics under /System/wifi/
            - Standardized hourly publish topic to /Cars/Hour/
            - Added retained season metadata publishes:
                System/seasonFolder, System/seasonYear
            - Added missing dual-beam timing telemetry:
                Sensors/beamAB_ms (A→B follow time)
                Sensors/beamB_broken_ms (duration both beams high)
            - Aligned TTP + timeBetweenCars publishes with GateCounter
            - Cleaned up beam debounce → publish only on stable state changes
25.11.24.2  Fixed incorrect brace nesting in MQTT callback that caused
                SHOWSTARTTIME and SHOWENDTIME handlers to be unreachable.
             Replaced all raw payload parsing (`atoi((char*)payload)`) with
                safe, null-terminated `message` buffer throughout callback.
             Hardened ShowStartDate handler with full YYYY-MM-DD validation and
                idempotency guard to ignore unchanged dates and prevent SD churn.
             Added min/max clamps to carCounterTimeout and waitDuration to avoid
                invalid or extreme configuration values being applied.
             Ensured all callback branches close cleanly and improved config
                update logging for clarity and stability during HA retained
                publishes. Changed some HELLO messages to DEBUG_LOG for legacy
             Set retained to true for beam sensor state publishes.
             Set retained to true for timeBetweenCars publishes and Initialized at boot.
25.11.23.1  Added season-based SD folder structure (/CC/YYYY/) and updated all
                SD read/write functions to use seasonal paths. Implemented
                determineSeasonYear() and ensureSeasonFolderExists() on boot.
             Added computeDaysRunning() integration with ShowStartDate MQTT
                subscription; DaysRunning now fully computed instead of incremented.
             Updated getSavedValuesOnReboot() and timeTriggeredEvents() to use
                computed DaysRunning (Christmas Eve gap preserved).
             Added auto-create /data directory at boot and inside upload handlers
                to prevent UI lockout conditions.
             Consolidated root route into a single safe fallback handler; added
                /showSummary.html route and seasonal /ShowSummary.csv serving.
             Updated index.html with “View Show Summary” button.
             Fixed duplicate global (showStartDateValid) and function ordering
                issues with forward declarations.

25.11.23.0  Restored proper 10-minute temperature/humidity publish interval
                (per 24.12.16.1) by removing all temp/RH MQTT publishes from
                readTempandRH() and KeepMqttAlive(). Added dedicated 10-minute
                temp/RH publish timer with retained JSON format.
             Added independent KeepMqttAlive() timer so heartbeat and retained
                count updates fire every 30 seconds regardless of other publishes;
                eliminated starvation caused by publishMQTT() resetting
                start_MqttMillis.
             Updated mqttKeepAlive variable comment to reflect heartbeat interval
                only (no longer tied to temp publishing).
             General cleanup of temp/RH logic, reduced MQTT spam, ensured only
                retained temp/RH JSON is published at 10-minute cadence.
25.11.22.3  Removed heatbeat MQTT topic ccountcar() to KeepMqttAlive() function to publish current counts
             every 30 seconds if no car is counted, ensuring remote dashboards
25.11.22.2  Added boot timestamp capture using RTC/NTP and published
             as part of the retained ONLINE banner. Added new retained
             heartbeat topic including boot time, current RTC time,
             enter/show counts, and WiFi RSSI for remote diagnostics.
             Standardized temp/humidity JSON publish on connect.
             Fixed publishDebugEvent() JSON time field to use RTC time.
             No changes to car-detection logic.
25.11.22.0 Bug fixes for MQTT publishes of temp & humidity as JSON
25.11.21.3 GAL: Updated WiFi setup for ESP32; removed unsupported setMinimumRSSI;
                 added primary-first connection logic with WiFiMulti fallback. preferPrimaryIfAvailable()
25.11.21.2 GAL: Changed order of SSID's in Secrets File
25.11.21.1 GAL: Restored idle playPattern() timeout; added retained MQTT publishes
                 for counts/WiFi/buckets; added retain-capable publishMQTT(),
                 expanded MQTT queue with overflow protection and chunked flush.
25.11.18.1 GAL: Added sdAvailable flag; removed SD-related while(1) lockups so device runs without SD
24.12.19.4 removed temperature array from average procedure an used the global declared array
24.12.19.3 Accidentally removed mqtt_client.setCallback(callback) Fixed with a forward declaration
24.12.19.2 added saveHourlyCounts() to countTheCar and removed from OTA Update
24.12.19.1 Refactored to match Gate Counter where common code exists
24.12.18.3 copied readTempandRH from gate counter
24.12.18.2 Moved saveHoulyCounts to timeTriggeredEvents & refactored
24.12.18.1 Renamed hourlyCarCount[] to hourlyCount[] and finished comparison to Gate Counter Code
24.12.17.2 added new topic MQTT_COUNTER_LOG "msb/traffic/CarCounter/CounterLog" used SaveHourlyCounts() from GateCounter
24.12.17.1 more tweaks to detectCar() revised averageHourlyTemp() & readTempandRH() removed averageHourlyTemp() from Loop changed downloadSDFile
24.12.16.1 removed all references to tempF from RTC sensor moved to DHT22 sensor publish every 10 min publish temp & RH json format
24.15.15.2 Changed SaveShowSummmary() to only average temps during show hours 
24.15.15.1 added SD.begin(PIN_SPI_CS) and DHT22 for temp & RH
24.12.11.5 OTA-SDCard Branch added endpoint and page to list /getShowSummary. Fixed uploading to subdirectory
24.12.11.4 OTA-SDCard Branch revised procedure for creating hourly filename 
24.12.11.3 OTA-SDCard Branch Added OTA for file operations Revised checkAndCreateFile() for server files needed under /data  
24.12.11.2 Added thousands separator to total show cars with new proceedure formatK
24.12.11.1 Refactored saveDailyShowSummary() to fix avg temp and put any cars up tp 9:10 pm into the 9:00 bucket & included show total & days running
24.12.10.3 Removed saveDailySummary(); Deleted global variables for hourly totals
24.12.10.2 Fixed topic in beamCarDetect for Hourly counts. Fixed if statement in timeTriggeredEvents()
24.12.10.1 Clarified procedure names, restructured midnight resets and hourly counts
24.12.09.1 Only updating temp readings once per hour, Fixed Display, added TotalDailyCars=0 at midnight, refactored hourly totals
24.12.08.1 Refactoring attempt to prevent lockups on overnight updates
24.12.04.7 Added minimum activation time for bounce detection
24.12.04.6 Afternoon logic added back in good and stable. Had time to add back 50ms debounce for snow conditions. Traffic too light to stress test
24.12.04.5 Removed Sensor Bounce Logic due to time constraints. Going back to afternoon logic.
24.12.04.4 Added MQTT_SUB_TOPIC6 to change beam sensor duration on the fly
24.12.04.3 added debounce to sensors for snow and changed stage machine debug messages. Increased waitDuration to 950ms
24.12.04.2 added queue to store unpublished MQTT Messages if MQTT server not connected.
24.12.04.1 Significant changes. Added State Machine for PlayPattern & Car Detection
24.12.03.1 Changed defines for MQTT Topics. Changed InitSD and removed duplicate file creation
24.11.27.4 moved MQTT updated outside file checks. Changed tempF to Float
24.11.27.3 changed reset counts to 5:00 pm and publish Array index daily total
24.11.27.2 changed tempF from int16_t to int
24.11.27.1 moved publishing days running to MQTT Connect. Changed delay after wifi connect from 5 sec to 1 sec
24.11.26.3 added days running to keepMQTTAlive
24.11.26.2 Changed Update to days running since they were doubling on date change
24.11.26.1 Removed running days from keepMQTT alive
24.11.25.1 Cleaned up MQTT Topics
24.11.24.2 Fixed Update fields for MQTT to be able to reset remotely
24.11.24.1 Changed field order for DailySummary Headers and Data. Appears working for Run-Walk
24.11.23.1 Alarm added after 1 minute will count car when cleared
24.11.22.2 Removed Alarm...until figured out
24.11.22.1 Added MQTT Message and Alarm if 2nd beam gets stuck
24.11.21.1 Added MQTT Messages for Reset and Daily Summary File Updates
24.11.19.1 Added boolean to print daily summary once
24.11.18.1 Added method to update counts using MQTT, Added additional topics for Show Totals, days running
24.11.16.4 16.3 didn't write daily summary. Changed that code for test 11.17
24.11.16.3 Changed write daily summary printing dayHour array
24.11.16.2 Added Time to Pass topic. Write totals not working.
24.11.16.1 Trying reset for 2nd beam timer, Fixed serial print logging
24.11.15.3 Fixed show minutes to report cars during show, increase car detect millis to 750
24.11.15.2 Cleaned up Data Files on SD Card. Archived old data set new start date
24.11.15.1 Updated MQTT Topics, File structures, aligned with Gate Counter
24.11.13.1 Added mqtt_client.loop() to while loop
24.11.10.1 fixed endless loop when second beam tripped. Mis formatting issues
24.11.9.1 Bugfix when both timers on for less than 500 ms
24.11.4.1 Exclude Christmas eve from Days Running, Changed name of Void to write daily totals, added config for showTime
24.11.3.4 Changed output file names and time to pass millis in carlog
24.11.3.3 dynamically created mqtt topic to publish totals and saving those totals to dayHour[24]
24.11.3.2 replace myFile2 with myFile
24.11.3.1 Changed Detector to Beam, re-wrote car detection logic, added MQTT publish Topic 10 beam state
24.11.2.1 Working Copy without hourly totals
24.10.25.1 Turn arches on during show added to end of loop
24.10.24.2 fixed publish dailytot on correct topic to keep gate counter in sync
24.10.24.1 Added enable arches between 4:30 pm & 9:30 pm
24.10.23.5 Bug fixes with missing {} clarified procedure names, added reset at midnight
24.10.23.4 Added update/reset check in loop for date changes. Created initSDCard()
24.10.23.3 Added update/reset check in loop for date changes
24.10.23.2 Updated totals, bug fixes, files ops comparison to Gate counter 
24.10.23.1 Updated totals, bug fixes, files ops comparison to Gate counter 
24.10.22.4 Testing Elegant OTA auto reboot
24.10.22.3 Added & debugged mqtt keep alive timer
24.10.21 Multiple bug fixes including inverting beam signals, Display, MQTT Keep Alive
24.10.21 Include FS.h
24.10.19 Used the ESP32 MQTT Program as a base and merged Andrew Bubb's code into it. The sdin, sdout
and serial writes used in the Arduino program were impossible to correct for ESP32. Spent
12 Hours before the program would compile. Problems may exist with the counts but ready for debugging. 10/20/24 GAL
24.10.17.2 
23.12.15 Added MQTT to Hive MQTT and Fixed WIFI for Testing at home and on-site7 12/15/23
23.12.13 Changed time format YYYY-MM-DD hh:mm:ss
Uses the existing car counter built by Andrew Bubb and outputs data to MQTT
## END CHANGELOG ##
*/
```
