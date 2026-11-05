#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.
void checkSdHealth() {
    // If SD was already offline, just report it
    if (!sdAvailable) {
        publishMQTT(MQTT_PUB_SD_HEALTH, "OFFLINE", true);
        publishMQTT(MQTT_DEBUG_LOG, "SD Health: sdAvailable=false (offline)");
        return;
    }

    // Try to write a temporary test file
    File test = recordingOpen("/_sd_test.tmp", FILE_WRITE);
    if (!test) {
        publishMQTT(MQTT_PUB_SD_HEALTH, "WRITE_FAIL", true);
        publishMQTT(MQTT_DEBUG_LOG,
                    "SD Health: WRITE TEST FAILED → marking sdAvailable=false");
        sdAvailable = false;
        return;
    }

    test.print("1");
    test.close();

    // Delete test file
    recordingRemove("/_sd_test.tmp");

    // If we got here, the card is good
    publishMQTT(MQTT_PUB_SD_HEALTH, "OK", true);
    publishMQTT(MQTT_DEBUG_LOG, "SD Health: OK");
}

void publishSdDiag_(const char* step, const char* file, const char* mode, const char* err) {

    bool mounted = (SD.cardType() != CARD_NONE);
    bool dataDirExists = SD.exists("/data");
    bool uiIndexExists = SD.exists("/data/index.html");  // adjust later if needed

    uint64_t totalMB = SD.totalBytes() / (1024ULL * 1024ULL);
    uint64_t usedMB  = SD.usedBytes()  / (1024ULL * 1024ULL);
    uint64_t freeMB  = (totalMB > usedMB) ? (totalMB - usedMB) : 0;

    char diag[220];
    snprintf(diag, sizeof(diag),
        "{\"mounted\":%s,\"step\":\"%s\",\"file\":\"%s\",\"mode\":\"%s\",\"err\":\"%s\","
        "\"dataDir\":%s,\"uiIndex\":%s,\"freeMB\":%llu}",
        mounted ? "true" : "false",
        step ? step : "",
        file ? file : "",
        mode ? mode : "",
        err  ? err  : "",
        dataDirExists ? "true" : "false",
        uiIndexExists ? "true" : "false",
        (unsigned long long)freeMB
    );

    // Use the existing buffer directly; no need to wrap in String()
    publishMQTT(MQTT_PUB_SD_DIAG, diag, true);
}

void ensureSeasonFolderExists() {
    int seasonYear = determineSeasonYear();
    snprintf(seasonFolder, sizeof(seasonFolder), "/CC/%04d", seasonYear);

    if (!SD.exists("/CC")) {
        recordingMkdir("/CC");
        publishMQTT(MQTT_DEBUG_LOG, "Created /CC directory");
    }

    if (!SD.exists(seasonFolder)) {
        recordingMkdir(seasonFolder);
        publishMQTT(MQTT_DEBUG_LOG, String("Created season folder: ") + seasonFolder);
    }

    publishMQTT(MQTT_DEBUG_LOG, String("Using season folder: ") + seasonFolder);
}

void getDailyTotal() {
    if (!sdAvailable) {
        Serial.printf("getDailyTotal skipped - SD not available\n");
        publishMQTT(MQTT_DEBUG_LOG,
                    "getDailyTotal skipped - SD not available (leaving totalDailyCars unchanged)");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName1);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = recordingOpen(fullPath, FILE_READ);

    if (myFile) {
        while (myFile.available()) {
            totalDailyCars = myFile.parseInt();   // read total
        }
        myFile.close();

        Serial.print("Daily cars from file = ");
        Serial.println(totalDailyCars);

        publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars));
    }
    else {
        Serial.print("SD Card: Cannot open file: ");
        Serial.println(fullPath);
        publishMQTT(MQTT_DEBUG_LOG,
                    String("getDailyTotal: Cannot open file: ") + fullPath);
        // NOTE: totalDailyCars left unchanged on failure
    }
}

void getShowTotal() {
    if (!sdAvailable) {
        Serial.printf("getShowTotal skipped - SD not available\n");
        publishMQTT(MQTT_DEBUG_LOG,
                    "getShowTotal skipped - SD not available (leaving totalShowCars unchanged)");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName2);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = recordingOpen(fullPath, FILE_READ);
    if (myFile) {
        while (myFile.available()) {
            totalShowCars = myFile.parseInt();  // read total
        }
        myFile.close();

        Serial.print("Total Show cars from file = ");
        Serial.println(totalShowCars);

        publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
    } 
    else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);

        publishMQTT(MQTT_DEBUG_LOG,
                    String("getShowTotal: Cannot open file: ") + fullPath);
        // NOTE: totalShowCars left unchanged on failure
    }
}

void getDayOfMonth() {
    if (!sdAvailable) {
        Serial.printf("getDayOfMonth skipped - SD not available\n");
        publishMQTT(MQTT_DEBUG_LOG,
                    "getDayOfMonth skipped - SD not available (leaving lastDayOfMonth unchanged)");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName3);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = recordingOpen(fullPath, FILE_READ);
    if (myFile) {
        while (myFile.available()) {
            lastDayOfMonth = myFile.parseInt();   // read day number
        }
        myFile.close();

        Serial.print("Calendar Day = ");
        Serial.println(lastDayOfMonth);

        publishMQTT(MQTT_PUB_DAYOFMONTH, String(lastDayOfMonth));
    } 
    else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);

        publishMQTT(MQTT_DEBUG_LOG,
                    String("getDayOfMonth: Cannot open file: ") + fullPath);
        // NOTE: lastDayOfMonth left unchanged on failure
    }
}

void getDaysRunning() {
    if (!sdAvailable) {
        Serial.printf("getDaysRunning skipped - SD not available\n");
        publishMQTT(MQTT_DEBUG_LOG,
                    "getDaysRunning skipped - SD not available (leaving daysRunning unchanged)");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName4);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = recordingOpen(fullPath, FILE_READ);
    if (myFile) {
        while (myFile.available()) {
            daysRunning = myFile.parseInt();  // read day number
        }
        myFile.close();

        Serial.print("Days Running (restored) = ");
        Serial.println(daysRunning);

        publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
    } 
    else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);

        publishMQTT(MQTT_DEBUG_LOG,
                    String("getDaysRunning: Cannot open file: ") + fullPath);

        // NOTE: daysRunning is LEFT UNCHANGED on failure
        // (computeDaysRunning() will supply a correct value)
    }
}

void getHourlyData() {
    if (!sdAvailable) {
        Serial.println("getHourlyData skipped - SD not available");
        publishMQTT(MQTT_DEBUG_LOG,
                    "getHourlyData skipped - SD not available (leaving hourlyCount unchanged)");
        return;
    }

    DateTime now = rtc.now();
    char dateBuffer[13];
    snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d",
             now.year(), now.month(), now.day());

    // -------------------------------
    // Normalize seasonFolder + file
    // -------------------------------
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName5);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    // -------------------------------
    // Open file
    // -------------------------------
    File file = recordingOpen(fullPath, FILE_READ);
    if (!file) {
        Serial.println("Failed to open hourly file. Resetting hourly data.");
        publishMQTT(MQTT_DEBUG_LOG,
                    "Failed to open hourly file. Resetting hourly data.");
        memset(hourlyCount, 0, sizeof(hourlyCount)); // Reset to zeros
        return;
    }

    bool rowFound = false;

    // Read the file line by line
    while (file.available()) {
        String line = file.readStringUntil('\n');

        if (line.startsWith(dateBuffer)) {
            rowFound = true;

            int parsedValues = sscanf(
                line.c_str(),
                "%*[^,],%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                &hourlyCount[0], &hourlyCount[1], &hourlyCount[2], &hourlyCount[3],
                &hourlyCount[4], &hourlyCount[5], &hourlyCount[6], &hourlyCount[7],
                &hourlyCount[8], &hourlyCount[9], &hourlyCount[10], &hourlyCount[11],
                &hourlyCount[12], &hourlyCount[13], &hourlyCount[14], &hourlyCount[15],
                &hourlyCount[16], &hourlyCount[17], &hourlyCount[18], &hourlyCount[19],
                &hourlyCount[20], &hourlyCount[21], &hourlyCount[22], &hourlyCount[23]
            );

            if (parsedValues == 24) {
                Serial.println("Successfully loaded hourly data for today.");
                publishMQTT(MQTT_DEBUG_LOG,
                            "Successfully loaded hourly data for today.");
            }
            else {
                Serial.println("Error parsing today's row. Resetting hourly data.");
                publishMQTT(MQTT_DEBUG_LOG,
                            "Error parsing today's row. Resetting hourly data.");
                memset(hourlyCount, 0, sizeof(hourlyCount));
            }

            break; // done after today's row
        }
    }

    file.close();

    if (!rowFound) {
        Serial.println("No data for today found. Resetting hourly data.");
        publishMQTT(MQTT_DEBUG_LOG,
                    "No data for today found. Resetting hourly data.");
        memset(hourlyCount, 0, sizeof(hourlyCount));
    }
}

void saveDailyTotal() {
    if (!sdAvailable) {
        Serial.println("saveDailyTotal skipped - SD not available");
        // GAL 25-11-26.x: still publish MQTT even if SD is unavailable
        publishMQTT(MQTT_DEBUG_LOG,
                    "saveDailyTotal skipped - SD not available (MQTT only)");
        publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars));
        return;
    }

    // Build full seasonal path safely (fileName1 already starts with '/')
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fullPath = folder + fileName1;

    myFile = recordingOpen(fullPath, FILE_WRITE);  // overwrite same as before
    if (myFile) {
        myFile.print(totalDailyCars);
        myFile.close();
    } else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);
        publishMQTT(MQTT_DEBUG_LOG,
                    String("SD Write Failure: Unable to save daily total: ") + fullPath);
    }

    // Always keep MQTT in sync with the in-RAM total
    publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars));
}

void saveShowTotal() {

    if (!sdAvailable) {
        Serial.println("saveShowTotal skipped - SD not available");
        publishSdDiag_("open", "", "w", "skip_sdUnavailable");

        // GAL 25-11-26.x: still publish MQTT total even if SD is unavailable
        publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
        return;
    }
    
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName2);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    // ---- OPEN ----
    myFile = recordingOpen(fullPath, FILE_WRITE);  // overwrite, same as before
    if (!myFile) {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);

        publishMQTT(MQTT_DEBUG_LOG,
            String("SD FAIL saveShowTotal open: ") + fullPath, false);

        publishSdDiag_("open", fullPath.c_str(), "w", "open_fail");
        // still publish MQTT total even if SD fails
        publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
        return;
    }

    // ---- WRITE ----
    size_t n = myFile.print(totalShowCars);  // only count cars between 4:55 pm and 9:10 pm

    // ---- FLUSH/CLOSE ----
    myFile.flush();
    myFile.close();

    if (n == 0) {
        publishMQTT(MQTT_DEBUG_LOG,
            String("SD FAIL saveShowTotal write0: ") + fullPath, false);

        publishSdDiag_("write", fullPath.c_str(), "w", "write_0");
    } else {
        // Optional: breadcrumb success (retained)
        publishSdDiag_("write", fullPath.c_str(), "w", "ok");
    }

    publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
}

void saveDayOfMonth() {
    if (!sdAvailable) {
        Serial.println("saveDayOfMonth: SD not available, MQTT only");
    } else {
        // GAL 25-11-26.1: normalize path join to avoid double slashes
        String folder = String(seasonFolder);
        if (folder.endsWith("/")) folder.remove(folder.length() - 1);

        String fname = String(fileName3);
        if (!fname.startsWith("/")) fname = "/" + fname;

        String fullPath = folder + fname;

        myFile = recordingOpen(fullPath, FILE_WRITE);  // overwrite, same as before
        if (myFile) {
            myFile.print(dayOfMonth);
            myFile.close();
        } else {
            Serial.print(F("SD Card: Cannot open the file: "));
            Serial.println(fullPath);
            publishMQTT(MQTT_DEBUG_LOG,
                "SD Write Failure: Unable to save day of month.");
        }
    }

    // Always publish MQTT
    publishMQTT(MQTT_PUB_DAYOFMONTH, String(dayOfMonth));
}

void saveDaysRunning() {
    // Always publish MQTT, even if SD is unavailable
    if (!sdAvailable) {
        Serial.println("saveDaysRunning: SD not available, MQTT only");
    } else {
        String folder = String(seasonFolder);
        if (folder.endsWith("/")) folder.remove(folder.length() - 1);

        String fname = String(fileName4);
        if (!fname.startsWith("/")) fname = "/" + fname;

        String fullPath = folder + fname;

        myFile = recordingOpen(fullPath, FILE_WRITE);
        if (myFile) {
            myFile.print(daysRunning);
            myFile.close();
        } else {
            Serial.print(F("SD Card: Cannot open the file: "));
            Serial.println(fullPath);
            publishMQTT(MQTT_DEBUG_LOG,
                "SD Write Failure: Unable to save days running.");
        }
    }

    // MQTT publish should ALWAYS happen
    publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
}

void saveHourlyCounts() {
    DateTime now = rtc.now();
    char dateBuffer[13];
    snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d",
             now.year(), now.month(), now.day());

    int currentHour = now.hour(); // Get the current hour (0-23)

    char topic[60];

    // GAL 25-11-26.x: Even if SD is not available, we still publish the hourly MQTT value.
    if (!sdAvailable) {
        Serial.println("saveHourlyCounts skipped - SD not available (MQTT only)");

        snprintf(topic, sizeof(topic),
                 "%s%02d", MQTT_PUB_CARS_HOURLY, currentHour);
        publishMQTT(topic, String(hourlyCount[currentHour]));

        publishMQTT(MQTT_DEBUG_LOG,
                    "saveHourlyCounts: SD not available; published MQTT only for current hour.");

        return;
    }

    // -------------------------------------------------
    // Normalize folder + fileName5
    // -------------------------------------------------
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName5);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    // -------------------------------------------------
    // READ EXISTING FILE
    // -------------------------------------------------
    File file = recordingOpen(fullPath, FILE_READ);
    String updatedContent = "";
    bool rowExists = false;

    if (file) {
        while (file.available()) {
            String line = file.readStringUntil('\n');

            if (line.startsWith(dateBuffer)) {
                rowExists = true;
                updatedContent += dateBuffer;

                int lastCommaIndex = line.indexOf(",") + 1;

                for (int i = 0; i < 24; i++) {
                    int nextCommaIndex = line.indexOf(",", lastCommaIndex);
                    String currentValue = (nextCommaIndex != -1)
                        ? line.substring(lastCommaIndex, nextCommaIndex)
                        : line.substring(lastCommaIndex);

                    lastCommaIndex = (nextCommaIndex != -1)
                        ? nextCommaIndex + 1
                        : lastCommaIndex;

                    updatedContent += (i == currentHour)
                        ? "," + String(hourlyCount[i])
                        : "," + currentValue;
                }

                updatedContent += "\n";

                // Publish current hour via MQTT
                snprintf(topic, sizeof(topic),
                        "%s%02d", MQTT_PUB_CARS_HOURLY, currentHour);
                publishMQTT(topic, String(hourlyCount[currentHour]));

            } else {
                updatedContent += line + "\n";
            }
        }
        file.close();
    }

    // -------------------------------------------------
    // NO ROW FOR TODAY → CREATE NEW ROW
    // -------------------------------------------------
    if (!rowExists) {
        updatedContent += dateBuffer;
        for (int i = 0; i < 24; i++) {
            updatedContent += (i == currentHour)
                ? "," + String(hourlyCount[i])
                : ",0";
        }
        updatedContent += "\n";

        snprintf(topic, sizeof(topic),
                 "%s%02d", MQTT_PUB_CARS_HOURLY, currentHour);
        publishMQTT(topic, String(hourlyCount[currentHour]));

        char debugMessage[100];
        snprintf(debugMessage, sizeof(debugMessage),
                 "Hourly data saved for hour %02d.", currentHour);
        publishMQTT(MQTT_DEBUG_LOG, debugMessage);
    }

    // -------------------------------------------------
    // WRITE UPDATED FILE BACK
    // -------------------------------------------------
    file = recordingOpen(fullPath, FILE_WRITE);
    if (file) {
        file.print(updatedContent);
        file.close();
        Serial.printf("Hourly counts for hour %02d saved and published.\n", currentHour);
    } else {
        Serial.println("Failed to open hourly file for writing.");
        publishMQTT(MQTT_DEBUG_LOG,
                    "SD Write Failure: Unable to save hourly counts.");
    }
}

void saveDailyShowSummary() {
    if (!sdAvailable) {
        Serial.println("saveDailyShowSummary skipped - SD not available");
        // GAL 25-11-26.x: this summary is for manual SD-based spreadsheets only,
        // so we do NOT publish MQTT_PUB_SUMMARY when SD is offline.
        publishMQTT(MQTT_DEBUG_LOG,
                    "saveDailyShowSummary skipped - SD not available (no summary written).");
        return;
    }

    DateTime now = rtc.now();

    // Calculate cumulative totals for each key hour
    int cumulative6PM = hourlyCount[17];                 // Total at 6 PM
    int cumulative7PM = cumulative6PM + hourlyCount[18]; // Total at 7 PM
    int cumulative8PM = cumulative7PM + hourlyCount[19]; // Total at 8 PM
    int cumulative9PM = cumulative8PM + hourlyCount[20]; // Total at 9 PM

    // Calculate total cars counted before the show starts
    int totalBefore5 = 0;
    for (int i = 0; i < 17; i++) {
        totalBefore5 += hourlyCount[i];
    }

    // Include additional cars detected between 9:00 PM and 9:20 PM
    if (now.hour() * 60 + now.minute() <= showEndMin) {
        cumulative9PM += hourlyCount[21];
    }

    // Calculate the average temperature during show hours (5 PM to 9 PM)
    float showTempSum = 0.0;
    int showTempCount = 0;
    for (int i = 17; i <= 20; i++) {
        if (hourlyTemp[i] != 0.0) {
            showTempSum += hourlyTemp[i];
            showTempCount++;
        }
    }
    float showAverageTemp = (showTempCount > 0) ? (showTempSum / showTempCount) : 0.0;

    // -------------------------------------------------
    // Build full seasonal path safely: /CC/YYYY/<fileName7>
    // -------------------------------------------------
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName7);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    // Open file for appending
    File summaryFile = recordingOpen(fullPath, FILE_APPEND);
    if (!summaryFile) {
        Serial.print("Failed to open daily show summary file: ");
        Serial.println(fullPath);
        publishMQTT(MQTT_DEBUG_LOG,
                    "Failed to open daily show summary file: " + fullPath);
        return;
    }

    // Format the current date
    char dateBuffer[13];
    snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d",
             now.year(), now.month(), now.day());

    // Append the show summary data
    summaryFile.printf("%s,%d,%d,%d,%d,%d,%d,%d,%.1f\n",
                       dateBuffer,
                       daysRunning,
                       totalBefore5,
                       cumulative6PM,
                       cumulative7PM,
                       cumulative8PM,
                       cumulative9PM,
                       totalShowCars,
                       showAverageTemp);
    summaryFile.close();

    // ---------------------------------------------
    // NEW: write bucket-style summary to second file
    // /CC/YYYY/ShowSummaryBuckets.csv (fileName10)
    // ---------------------------------------------

    // Compute per-hour buckets from cumulative values
    int bucket6PM = cumulative6PM;
    int bucket7PM = cumulative7PM - cumulative6PM;
    int bucket8PM = cumulative8PM - cumulative7PM;
    int bucket9PM = cumulative9PM - cumulative8PM;

    // Full show total for the night (includes any 9:00–9:20 bump already
    // rolled into cumulative9PM above)
    int dailyTotal = cumulative9PM;

    // Build seasonal path: /CC/YYYY/ShowSummaryBuckets.csv
    String fnameBuckets = String(fileName10);
    if (!fnameBuckets.startsWith("/")) fnameBuckets = "/" + fnameBuckets;
    String fullPathBuckets = folder + fnameBuckets;

    File bucketFile = recordingOpen(fullPathBuckets, FILE_APPEND);
    if (!bucketFile) {
        Serial.print("Failed to open bucketed show summary file: ");
        Serial.println(fullPathBuckets);
        publishMQTT(MQTT_DEBUG_LOG,
                    "Failed to open bucketed show summary file: " + fullPathBuckets);
        // Do NOT return here; cumulative summary already written.
    } else {
        // Format: Date,DaysRunning,6PM,7PM,8PM,9PM,DailyTotal,ShowTotal,DailyAvgTemp
        bucketFile.printf("%s,%d,%d,%d,%d,%d,%d,%d,%.1f\n",
                          dateBuffer,
                          daysRunning,
                          bucket6PM,
                          bucket7PM,
                          bucket8PM,
                          bucket9PM,
                          dailyTotal,
                          totalShowCars,
                          showAverageTemp);
        bucketFile.close();
    }
    
    // GAL 25-11-26.x:
    // This summary is used for manual spreadsheets from SD, not HA/Calendar.
    char debugBuf[160];
    snprintf(debugBuf, sizeof(debugBuf),
             "Daily show summary written: %s, DaysRunning: %d, Before5: %d, "
             "6PM: %d, 7PM: %d, 8PM: %d, 9PM: %d, ShowTotal: %d, AvgTemp: %.1f",
             dateBuffer, daysRunning, totalBefore5, cumulative6PM, cumulative7PM,
             cumulative8PM, cumulative9PM, totalShowCars, showAverageTemp);
    publishMQTT(MQTT_PUB_SUMMARY, debugBuf);

    Serial.printf("Daily show summary written: %s, DaysRunning: %d, Before5: %d, 6PM: %d, 7PM: %d, 8PM: %d, 9PM: %d, ShowTotal: %d, Avg Temp: %.1f°F.\n",
                  dateBuffer, daysRunning, totalBefore5, cumulative6PM, cumulative7PM,
                  cumulative8PM, cumulative9PM, totalShowCars, showAverageTemp);
}

void getSavedValuesOnReboot() {
    DateTime now = rtc.now();

    // GAL 25-11-18: If SD is offline, start fresh but keep running
    // GAL 25-11-26.x: Do NOT wipe DaysRunning; recompute from RTC + showStartDate.
    if (!sdAvailable) {
        Serial.println("getSavedValuesOnReboot: SD not available; using RTC + defaults.");

        // You can still compute these for internal use if you want:
        dayOfMonth    = now.day();
        lastDayOfMonth = dayOfMonth;

        if (showStartDateValid && rtcReady && now.year() >= 2024) {
            daysRunning = computeDaysRunning();
            publishMQTT(MQTT_DEBUG_LOG,
                "Reboot (no SD): DaysRunning recomputed (internal only) = " + String(daysRunning));
        } else {
            publishMQTT(MQTT_DEBUG_LOG,
                "Reboot (no SD): showStartDate not set; DaysRunning left at default.");
        }

        // BUT: do NOT call saveDayOfMonth() or saveDaysRunning() here.
        // They publish to MQTT and overwrite HA, which we do NOT want when SD is bad.

        publishMQTT(MQTT_DEBUG_LOG,
            "Reboot (no SD): Daily/show totals and hourly counts not restored; data lost. HA totals left unchanged.");

        return;
    }

    // SD is available from here down
    DateTime now2 = now;  // just to be explicit we’re using the same snapshot

    // Always restore show total, including the new-day boot branch.
    getShowTotal();

    // Load last day of month from SD
    getDayOfMonth();
    dayOfMonth = lastDayOfMonth;  // <<< ensure we always have a valid day before publishing

    // Did we cross a day boundary?
    if (now2.day() != lastDayOfMonth) {

        // Update dayOfMonth and save
        dayOfMonth = now2.day();
        saveDayOfMonth();

        // Reset daily totals (new day)
        totalDailyCars = 0;
        saveDailyTotal();
        publishMQTT(MQTT_DEBUG_LOG, "Rebooted: Daily totals reset for new day.");

        // ================================
        // GAL 25-11-23.x:
        // DaysRunning is now computed.
        // Christmas Eve skip preserved.
        // ================================
        if (showStartDateValid && rtcReady && now2.year() >= 2024) {
            int newDaysRunning = computeDaysRunning();
            daysRunning = newDaysRunning;
            saveDaysRunning();

            publishMQTT(MQTT_DEBUG_LOG,
                "Rebooted: DaysRunning recomputed = " + String(daysRunning));
        }
        else {
            // No showStartDate yet: load last known DaysRunning
            getDaysRunning();
            publishMQTT(MQTT_DEBUG_LOG,
                "Rebooted: showStartDate not set yet; DaysRunning held at "
                + String(daysRunning));
        }

        Serial.println("ESP32 reboot detected on a new day. Counts reset/updated.");
        publishMQTT(MQTT_DEBUG_LOG, "Rebooted: Counts reset/updated for new day.");
    }
    else {
        // Same day: reload everything
        getDailyTotal();
        getShowTotal();
        getDaysRunning();
        getHourlyData();
        dayOfMonth = lastDayOfMonth;  // <<< FIX: use the SD value we just loaded
        Serial.println("ESP32 reboot detected on same day. Reloading saved counts.");
        publishMQTT(MQTT_DEBUG_LOG, "Rebooted: Counts reloaded for same day.");
    }

    // After SD-based reload/update, make sure MQTT is in sync:
    totalsInitialized = true;   // <<-- add this, SD path only
    publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars));
    publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
    publishMQTT(MQTT_PUB_DAYOFMONTH, String(dayOfMonth));
    publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
    // If you want hourly buckets pushed too, this will write to SD and MQTT:
    // saveHourlyCounts();
}

void checkAndCreateFile(const String &fileName, const String &header) {
    if (!sdAvailable) {
        Serial.printf("checkAndCreateFile skipped (%s) - SD not available\n",
                      fileName.c_str());

        // GAL 25-11-26.x: Add explicit MQTT diagnostic for HA visibility
        publishMQTT(MQTT_DEBUG_LOG,
                    String("checkAndCreateFile skipped (SD missing): ") + fileName);
        return;
    }

    // GAL 25-11-23.2: /data/* files live at SD root; everything else in season folder
    String fullPath;

    if (fileName.startsWith("/data/")) {
        fullPath = fileName;  // absolute path at root (UI files)
    } else {
        // normalize seasonal join to avoid double slashes
        String folder = String(seasonFolder);
        if (folder.endsWith("/")) {
            folder.remove(folder.length() - 1);
        }

        // fileName already starts with "/" in your constants, but keep this safe
        String fname = String(fileName);
        if (!fname.startsWith("/")) {
            fname = "/" + fname;
        }

        fullPath = folder + fname;
    }

    if (!SD.exists(fullPath)) {
        Serial.printf("%s not found. Creating...\n", fullPath.c_str());

        if (fileName.endsWith("/")) { // Create directory if name ends with '/'
            if (!recordingMkdir(fullPath)) {
                Serial.printf("Failed to create directory %s\n", fullPath.c_str());
                publishMQTT(MQTT_DEBUG_LOG,
                            String("Failed to create directory: ") + fullPath);
            } else {
                Serial.printf("Directory %s created successfully\n", fullPath.c_str());
                publishMQTT(MQTT_DEBUG_LOG,
                            String("Directory created: ") + fullPath);
            }

        } else { // Create file
            File file = recordingOpen(fullPath, FILE_WRITE);
            if (!file) {
                Serial.printf("Failed to create file %s\n", fullPath.c_str());
                publishMQTT(MQTT_DEBUG_LOG,
                            String("Failed to create file: ") + fullPath);
            } else {
                if (!header.isEmpty()) {
                    file.println(header);
                }
                file.close();
                Serial.printf("File %s created successfully\n", fullPath.c_str());
                publishMQTT(MQTT_DEBUG_LOG,
                            String("File created: ") + fullPath);
            }
        }
    }
}

void createAndInitializeHourlyFile(const String &fileName) {

    // Build full seasonal path safely (fileName already starts with '/')
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fullPath = folder + fileName;

    if (!SD.exists(fullPath)) {
        Serial.printf("%s not found. Creating...\n", fullPath.c_str());

        File file = recordingOpen(fullPath, FILE_WRITE);
        if (!file) {
            Serial.printf("Failed to create file %s\n", fullPath.c_str());
            return;
        }

        // Write the header
        file.println("Date,Hr 00,Hr 01,Hr 02,Hr 03,Hr 04,Hr 05,Hr 06,Hr 07,Hr 08,Hr 09,Hr 10,Hr 11,Hr 12,Hr 13,Hr 14,Hr 15,Hr 16,Hr 17,Hr 18,Hr 19,Hr 20,Hr 21,Hr 22,Hr 23");

        // Add an initial row for the current day
        DateTime now = rtc.now();
        char dateBuffer[13];
        snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d", now.year(), now.month(), now.day());
        file.print(dateBuffer);

        // Initialize 24 hourly counts to 0
        for (int i = 0; i < 24; i++) {
            file.print(",0");
        }
        file.println();

        file.close();
        Serial.println("Hourly file created and initialized for the current day.");
    } else {
        Serial.printf("File %s already exists. No initialization performed.\n", fullPath.c_str());
    }
}

void initSDCard() {
    sdAvailable = false;  // pessimistic default

    // ---- SD MOUNT RETRY ----
    // GAL 25-11-23.2: some cards need delay/retry and lower SPI freq on ESP32
    bool mountedOK = false;
    for (int attempt = 1; attempt <= 5; attempt++) {
        delay(300);  // give card more wake-up time
        if (SD.begin(PIN_SPI_CS, SPI, 4000000)) {  // 4 MHz (more reliable)
            mountedOK = true;
            break;
        }
        publishMQTT(MQTT_DEBUG_LOG,
            String("SD mount attempt ") + attempt + " failed", false);
    }

    if (!mountedOK) {
        Serial.println("Card Mount Failed");

        // OLED quick indicator
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, line1);
        display.println("SD ERROR");
        display.display();

        publishMQTT(MQTT_PUB_SD_STATUS, "FAIL_MOUNT", true);
        publishMQTT(MQTT_PUB_SD_HEALTH, "OFFLINE", true);
        publishMQTT(MQTT_DEBUG_LOG, "SD FAIL: mount failed", false);

        publishSdDiag_("init", "", "", "mount_fail");
        return;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        publishMQTT(MQTT_PUB_SD_STATUS, "FAIL_NO_CARD", true);
        publishMQTT(MQTT_PUB_SD_HEALTH, "OFFLINE", true);
        publishMQTT(MQTT_DEBUG_LOG, "SD FAIL: no card", false);

        publishSdDiag_("init", "", "", "no_card");
        return;
    }

    // GAL 25-11-23.2: SD.totalBytes() is more reliable than cardSize() on ESP32 SPI
    uint64_t cardSizeMB = SD.totalBytes() / (1024ULL * 1024ULL);
    Serial.printf("SD Card Size: %lluMB\n", cardSizeMB);

    loadRecordingCalendarFromSD();

    // ---- Validate critical folders/files ----
    bool dataDirExists = SD.exists("/data");
    bool uiIndexExists = SD.exists("/data/index.html");   // if your UI entry file differs, we’ll adjust later

    // Ensure /CC exists
    bool ccDirExists = SD.exists("/CC");
    if (!ccDirExists) {
        ccDirExists = recordingMkdir("/CC");
        if (ccDirExists) {
            publishMQTT(MQTT_DEBUG_LOG, "Created /CC directory", false);
        } else {
            publishMQTT(MQTT_DEBUG_LOG, "SD FAIL: unable to create /CC", false);
        }
    }

    // Create season folder (uses your global seasonFolder buffer)
    int seasonYear = determineSeasonYear();
    snprintf(seasonFolder, sizeof(seasonFolder), "/CC/%04d", seasonYear);

    bool seasonDirExists = SD.exists(seasonFolder);
    if (!seasonDirExists) {
        seasonDirExists = recordingMkdir(seasonFolder);
        if (seasonDirExists) {
            publishMQTT(MQTT_DEBUG_LOG,
                String("Created season folder: ") + seasonFolder, false);
        } else {
            publishMQTT(MQTT_DEBUG_LOG,
                String("SD FAIL: unable to create season folder: ") + seasonFolder, false);
        }
    }

    // GAL 25-11-26: publish season folder + year (retained)
    if (seasonDirExists) {
        publishMQTT(MQTT_PUB_SEASON_FOLDER, String(seasonFolder), true);
        publishMQTT(MQTT_PUB_SEASON_YEAR,   String(seasonYear),   true);
    }

    // ---- Lightweight write/readback test ----
    bool writeTestOK = false;
    const char* testFile = "/sd_test.txt";

    File tf = recordingOpen(testFile, FILE_WRITE);
    if (tf) {
        size_t n = tf.print("test\n");
        tf.flush();
        tf.close();

        if (n > 0) {
            File rf = recordingOpen(testFile, FILE_READ);
            if (rf) {
                String s = rf.readStringUntil('\n');
                rf.close();
                writeTestOK = (s == "test");
            }
        }

    #if 1
        recordingRemove(testFile);
    #endif
    }

    // GAL 25-11-23.x: SD usable even if writeTest fails; writeTest is diagnostic only
    sdAvailable = (ccDirExists && seasonDirExists);

    // OLED indicator
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, line1);
    if (sdAvailable) {
        display.printf("SD OK: %lluMB", cardSizeMB);
    } else {
        display.println("SD DEGRADED");
    }
    display.display();

    // MQTT retained OK + size OR degraded flag
    if (sdAvailable) {
        publishMQTT(MQTT_PUB_SD_STATUS,
            String("OK_") + String(cardSizeMB) + "MB", true);
        publishMQTT(MQTT_PUB_SD_HEALTH, "OK", true);
        publishMQTT(MQTT_DEBUG_LOG,
            String("SD OK: ") + String(cardSizeMB) + "MB", false);
    } else {
        publishMQTT(MQTT_PUB_SD_STATUS, "FAIL_DEGRADED", true);
        publishMQTT(MQTT_PUB_SD_HEALTH, "DEGRADED", true);
        publishMQTT(MQTT_DEBUG_LOG, "SD FAIL: degraded (see sdDiag)", false);
    }

    // Retained structured diag
    char diag2[220];
    snprintf(diag2, sizeof(diag2),
        "{\"sizeMB\":%llu,\"dataDir\":%s,\"uiIndex\":%s,"
        "\"ccDir\":%s,\"seasonDir\":%s,\"writeTest\":%s}",
        cardSizeMB,
        dataDirExists ? "true" : "false",
        uiIndexExists ? "true" : "false",
        ccDirExists ? "true" : "false",
        seasonDirExists ? "true" : "false",
        writeTestOK ? "true" : "false"
    );
    publishMQTT(MQTT_PUB_SD_DIAG, diag2, true);
}


// Device adapter; the calendar policy itself is shared with Gate.
void applyRecordingCalendar() {
    DateTime start=recordingShowStart();
    showStartY=start.year(); showStartM=start.month(); showStartD=start.day();
    showStartDateValid=true;
    snprintf(seasonFolder,sizeof(seasonFolder),"/CC/%04d",recordingYear());
    if (SD.cardType()==CARD_NONE) return;
    ensureSeasonFolderExists();
    if (!sdAvailable) return;
    checkAndCreateFile(fileName1);
    checkAndCreateFile(fileName2);
    checkAndCreateFile(fileName3);
    checkAndCreateFile(fileName4);
    checkAndCreateFile(fileName6,"DateTime, TimeToPass_ms, EnterDailyTotal, AB_Follow_ms, TimeBetweenCars_ms");
    checkAndCreateFile(fileName7,"Date,DaysRunning,Before5,6PM,7PM,8PM,9PM,ShowTotal,DailyAvgTemp");
    checkAndCreateFile(fileName10,"Date,DaysRunning,6PM,7PM,8PM,9PM,DailyTotal,ShowTotal,DailyAvgTemp");
    createAndInitializeHourlyFile(fileName5);
}
