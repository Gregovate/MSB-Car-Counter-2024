# MSB-Car-Counter-2024
Car counter Interface to MQTT by Greg Liebig gliebig@sheboyganlights.org
Initial Build 12/5/2023 12:15 pm
Changed time format YYYY-MM-DD hh:mm:ss 12/13/23

Changelog
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
24.11.16.3 Changed write daily summar printing dayHour array
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
24.10.23.5 Bug fixes with missing {} clarified prodecure names, added reset at midnight
24.10.23.4 Added update/reset check in loop for date changes. Created initSDCard()
24.10.23.3 Added update/reset check in loop for date changes
24.10.23.2 Updated totals, bug fixes, files ops comparrison to Gate counter 
24.10.23.1 Updated totals, bug fixes, files ops comparrison to Gate counter 
24.10.22.4 Testing Elegant OTA auto reboot
24.10.22.3 Added & debugged mqtt keep alive timer
24.10.21 Multiple bug fixes including ineverting beam signals, Display, MQTT Keep Alive
24.10.21 Include FS.h
24.10.19 Used the ESP32 MQTT Program as a base and merged Andrew Bubb's code into it. The sdin, sdout
and serial writes used in the Arduino program were impossible to correct for ESP32. Spent
12 Hours before the program would compile. Problems may exist with the counts but ready for debugging. 10/20/24 GAL
24.10.17.2 
23.12.15 Added MQTT to Hive MQTT and Fixed WIFI for Testing at home and on-site7 12/15/23
Purpose: suppliments Car Counter to improve traffic control and determine park capacity
Counts vehicles as they exit the park
Connects to WiFi and updates RTC on Boot
Uses the existing car counter built by Andrew Bubb and outputs data to MQTT
Stores data on SD card in event of internet failure
DOIT DevKit V1 ESP32 with built-in WiFi & Bluetooth

#define topic_base_path  "msb/traffic/CarCounter"
#define MQTT_PUB_TOPIC0  "msb/traffic/CarCounter/hello"
#define MQTT_PUB_TOPIC1  "msb/traffic/CarCounter/temp"
#define MQTT_PUB_TOPIC2  "msb/traffic/CarCounter/time"
#define MQTT_PUB_TOPIC3  "msb/traffic/CarCounter/count"
#define MQTT_PUB_TOPIC4  "msb/traffic/CarCounter/Cars_18"
#define MQTT_PUB_TOPIC5  "msb/traffic/CarCounter/Cars_19"
#define MQTT_PUB_TOPIC6  "msb/traffic/CarCounter/Cars_20"
#define MQTT_PUB_TOPIC7  "msb/traffic/CarCounter/Cars_21"
#define MQTT_PUB_TOPIC8  "msb/traffic/CarCounter/EnterTotal"
#define MQTT_PUB_TOPIC9  "msb/traffic/CarCounter/ShowTotal"
#define MQTT_PUB_TOPIC10 "msb/traffic/CarCounter/secondBeamSensorState"
#define MQTT_PUB_TOPIC11 "msb/traffic/CarCounter/TTP"
#define MQTT_PUB_TOPIC12 "msb/traffic/CarCounter/DaysRunning"

// Subscribing Topics (to reset values)
#define MQTT_SUB_TOPIC0  "msb/traffic/CarCounter/EnterTotal"
#define MQTT_SUB_TOPIC1  "msb/traffic/CarCounter/resetDailyCount"
#define MQTT_SUB_TOPIC2  "msb/traffic/CarCounter/resetShowCount"
#define MQTT_SUB_TOPIC3  "msb/traffic/CarCounter/resetDayOfMonth"
#define MQTT_SUB_TOPIC4  "msb/traffic/CarCounter/resetDaysRunning"