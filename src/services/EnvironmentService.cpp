#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.
void averageHourlyTemp() {
    static int lastPublishedHour = -1;     // Tracks the last hour when data was published
    static int tempReadingsCount = 0;      // Number of valid temperature readings
    static float tempReadingsSum = 0.0;    // Sum of valid temperature readings
    

    // Get the current time
    DateTime now = rtc.now();
    int nowHour = now.hour();

    // Check if the hour has changed
    if (nowHour != lastPublishedHour) {
        // Publish the average for the completed hour
        if (tempReadingsCount > 0 && lastPublishedHour >= 0) {
            hourlyTemp[lastPublishedHour] = tempReadingsSum / tempReadingsCount;

            // Publish to MQTT
            char topic[50];
            snprintf(topic, sizeof(topic), "%s/hourly/%02d", MQTT_PUB_TEMP, lastPublishedHour);
            publishMQTT(topic, String(hourlyTemp[lastPublishedHour], 1)); // Publish with 1 decimal place

            // Log the published temperature
            Serial.printf("Hour %02d average temperature published: %.1f°F\n", lastPublishedHour, hourlyTemp[lastPublishedHour]);
            //publishDebugLog("Hourly average temperature published: " + String(hourlyTemp[lastPublishedHour], 1));
        }

        // Reset for the new hour
        lastPublishedHour = nowHour;
        tempReadingsSum = 0.0;
        tempReadingsCount = 0;
    }

    // Add the latest temperature reading if valid
    if (tempF != -999) { // Ensure only valid readings are processed
        tempReadingsSum += tempF;
        tempReadingsCount++;
        //publishDebugLog("Temperature added for averaging: " + String(tempF));
    }
}


