/*
Car counter Interface to MQTT by Greg Liebig greg@engrinnovations.com
Initial Build 12/5/2023 12:15 pm
Stores data on SD card in event of internet failure
DOIT DevKit V1 ESP32 with built-in WiFi & Bluetooth */

// IMPORTANT: Update FWVersion each time a new changelog entry is added
#define OTA_Title "Car Counter" // OTA Title
#define FWVersion "25.11.24.2-MQTTfix"

/* ## CAR COUNTER BEGIN CHANGELOG ##
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
                publishes.
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

#include <Arduino.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"
#include "NTPClient.h"
//#include "WiFiClientSecure.h"
#include <WiFiMulti.h>
#include "secrets.h"
#include "time.h"
#include "SD.h"
#include "FS.h"
#include "SPI.h"
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <AsyncTCP.h>
#endif
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTAPro.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <queue>  // Include queue for storing messages

// ******************** CONSTANTS *******************
#define DS3231_I2C_ADDRESS 0x68 // Real Time Clock
#define firstBeamPin 33
#define secondBeamPin 32
#define redArchPin 25
#define greenArchPin 26
#define DHTPIN 4       // GPIO pin for the DHT22
#define DHTTYPE DHT22  // DHT TYPE
#define PIN_SPI_CS 5   // The ESP32 pin GPIO5
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels


//CAR COUNTER GLOBAL CONSTANTS FOR SHOW TIMES and SAFETY
unsigned int waitDuration = 950; // minimum millis for secondBeam to be broken needed to detect a car
const int showStartMinDefault = 17*60;      // default 5:00 pm
const int showEndMinDefault   = 21*60 + 10; // default 9:10 pm

int showStartMin = showStartMinDefault;    // live value (HA can override)
int showEndMin   = showEndMinDefault;      // live value (HA can override later)
bool rtcReady = false;  // GAL 25-11-22: RTC boot-safety guard
String bootTimestamp = "";  // GAL 25-11-22: Store boot timestamp for logging
volatile bool pendingSaveDaily = false; // GAL 25-11-23.x: Flag to defer daily save to safe context
volatile bool pendingSaveShow  = false; // GAL 25-11-23.x: Flag to defer show summary save to safe context
volatile bool pendingSaveHourly = false; // GAL 25-11-24: Flag to defer hourly save to safe context

// GLOBAL VARIABLES and TIMERS
// GAL 25-11-22.4: independent timers for keepalive and temp publish
unsigned long lastKeepAliveMillis = 0;
const unsigned long TEMP_PUB_INTERVAL_MS = 10UL * 60UL * 1000UL;  // 10 minutes
unsigned long lastTempPubMillis = 0;
int mqttKeepAlive = 30;  // GAL 25-11-22.4: heartbeat interval (seconds)

// **************************************************


#define THIS_MQTT_CLIENT "espCarCounter" // Look at line 90 and set variable for WiFi Client secure & PubSubCLient 12/23/23

/***** MQTT TOPIC DEFINITIONS for Car Counter*****/

// Publishing Topics 
char topic[60];          // keep for dynamic hourly topic building
char topicBase[60];      // keep until confirmed unused

#define topic_base_path "msb/traffic/CarCounter"

/* System / Status */
#define MQTT_PUB_HELLO          "msb/traffic/CarCounter/System/hello"
#define MQTT_PUB_FW_VERSION     "msb/traffic/CarCounter/System/firmware"
#define MQTT_PUB_TIME           "msb/traffic/CarCounter/System/time"
#define MQTT_DEBUG_LOG          "msb/traffic/CarCounter/System/debug" 
#define MQTT_PUB_ALARM         "msb/traffic/CarCounter/System/alarm"

/* Environment (temp + humidity, retained JSON) */
#define MQTT_PUB_TEMP           "msb/traffic/CarCounter/Env/temp"

/* Car Counts */
#define MQTT_PUB_ENTER_CARS     "msb/traffic/CarCounter/Cars/EnterTotal"
#define MQTT_PUB_SHOWTOTAL      "msb/traffic/CarCounter/Cars/ShowTotal"
#define MQTT_PUB_CARS_HOURLY    "msb/traffic/CarCounter/Cars/hour"
#define MQTT_PUB_HOURLY_JSON    "msb/traffic/CarCounter/Cars/hourly_json"
#define MQTT_PUB_TTP            "msb/traffic/CarCounter/Cars/TTP"
#define MQTT_PUB_TIMEBETWEENCARS "msb/traffic/CarCounter/Cars/timeBetweenCars"

/* Calendar + Show Tracking */
#define MQTT_PUB_DAYOFMONTH     "msb/traffic/CarCounter/Calendar/dayOfMonth"
#define MQTT_PUB_DAYSRUNNING    "msb/traffic/CarCounter/Calendar/daysRunning"
#define MQTT_PUB_SUMMARY        "msb/traffic/CarCounter/Calendar/showSummary"

/* Beam Sensors */
#define MQTT_FIRST_BEAM_SENSOR_STATE   "msb/traffic/CarCounter/Sensors/beam1"
#define MQTT_SECOND_BEAM_SENSOR_STATE  "msb/traffic/CarCounter/Sensors/beam2"
#define MQTT_COUNTER_LOG "msb/traffic/CarCounter/Sensors/counterLog"

/* WiFi Diagnostics */
#define MQTT_PUB_RSSI          "msb/traffic/CarCounter/System/wifi/rssi"
#define MQTT_PUB_SSID          "msb/traffic/CarCounter/System/wifi/ssid"
#define MQTT_PUB_IP            "msb/traffic/CarCounter/System/wifi/ip"
#define MQTT_PUB_HEARTBEAT     "msb/traffic/CarCounter/System/heartbeat"


/* SD Diagnostics (Retained JSON) */
#define MQTT_PUB_SD_DIAG       "msb/traffic/CarCounter/System/sdDiag"
#define MQTT_PUB_SD_STATUS     "msb/traffic/CarCounter/System/sdStatus"

/* Echoed Config (useful for dashboards + Gate Counter offset) */
#define MQTT_PUB_SHOW_START_DATE "msb/traffic/CarCounter/Config/showStartDate"
#define MQTT_PUB_SHOW_START_MIN  "msb/traffic/CarCounter/Config/showStartMin"
#define MQTT_PUB_SHOW_END_MIN    "msb/traffic/CarCounter/Config/showEndMin"


/***** SUBSCRIBE TOPICS *****/

/* Show Start Parameters */
#define MQTT_SUB_SHOWSTART     "msb/traffic/CarCounter/Config/showStartDate"    // Set Show Start Date (YYYY-MM-DD)
#define MQTT_SUB_SHOWSTARTTIME "msb/traffic/CarCounter/Config/showStartTime"    // Set Show Start Time (HH:MM)
#define MQTT_SUB_SHOWENDTIME   "msb/traffic/CarCounter/Config/showEndTime"      // Set Show End Time (HH:MM)

/* Manual resets */
#define MQTT_SUB_TOPIC1        "msb/traffic/CarCounter/resetDailyCount"      // Reset Daily counter
#define MQTT_SUB_TOPIC2        "msb/traffic/CarCounter/resetShowCount"       // Resets Show Counter
#define MQTT_SUB_TOPIC3        "msb/traffic/CarCounter/resetDayOfMonth"      // Reset Calendar Day
#define MQTT_SUB_TOPIC4        "msb/traffic/CarCounter/resetDaysRunning"     // Reset Days Running
#define MQTT_SUB_TOPIC5        "msb/traffic/CarCounter/carCounterTimeout"    // Reset Timeout if car leaves detection Zone
#define MQTT_SUB_TOPIC6        "msb/traffic/CarCounter/waitDuration"         // Reset time from firstBeamSensor trip to secondBeamSensor Active


/** State Machine Enum to represent the different states of the play pattern */
enum PatternState {
    INITIAL_OFF,
    RED_ON,
    GREEN_ON,
    BOTH_ON,
    BOTH_OFF,
    RED_OFF_GREEN_ON,
    GREEN_OFF_RED_ON
};

// Pattern State Machine Variables
PatternState currentPatternState = INITIAL_OFF;
unsigned long patternModeMillis = 0;

/** State Machine Enum for Car Detection */
// Enum to represent the different states of car detection
enum CarDetectState {
    WAITING_FOR_CAR,
    FIRST_BEAM_HIGH,
    BOTH_BEAMS_HIGH,
    CAR_DETECTED
};

// Car Detection State Machine Variables
CarDetectState currentCarDetectState = WAITING_FOR_CAR;
bool carPresentFlag = false; // Flag to ensure a car is only counted once
unsigned long firstBeamTripTime = 0;
unsigned long bothBeamsHighTime = 0;
unsigned long lastCarDetectedMillis = 0;
unsigned long secondBeamTripTime = 0;

//const unsigned long personTimeout = 500;        // Time in ms considered too fast for a car, more likely a person
//const unsigned long bothBeamsHighThreshold = 750;  // Time in ms for both beams high to consider a car is in the detection zone

unsigned long timeToPassMS = 0;
int firstBeamState;  // Holds the current state of the FIRST IR receiver/Beam
int secondBeamState;  // Holds the current state of the SECOND IR receiver/Beam
int prevFirstBeamState = -1; // Holds the previous state of the FIRST IR receiver/Beam
int prevSecondBeamState = -1; // Holds the previous state of the SECOND IR receiver/Beam 
unsigned long debounceDelay = 50; // 50 ms debounce delay
unsigned long minActivationDuration = 100; // 100ms Minimum duration for valid activation

// Variables to store the stable state and last state change time
bool stableFirstBeamState = LOW;
bool stableSecondBeamState = LOW;
unsigned long lastFirstBeamChangeTime = 0;
unsigned long lastSecondBeamChangeTime = 0;

AsyncWebServer server(80);     // Define Webserver
String currentDirectory = "/"; // Current working directory

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  //saveHourlyCounts();
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

/** REAL TIME Clock & Time Related Variables **/
RTC_DS3231 rtc;
const char* ampm ="AM";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;
const int   daylightOffset_sec = 3600;

// GAL 25-11-22: DS3231 timestamp helper (use for GateCounter HELLO/debug)
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

// Initialize DHT sensor & Variables for temperature and humidity readTempandRH() and show start 
DHT dht(DHTPIN, DHTTYPE);
float tempF = 0.0;      // Temperature
float humidity = 0.0;   // Humidity
int showStartY = 0;     // Show Start Year
int showStartM = 0;     // Show Start Month
int showStartD = 0;     // Show Start Day
bool showStartDateValid = false;

/** Display Definitions & variables **/
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
/*Line Numbers used for Display*/
const int line1 =0;
const int line2 =9;
const int line3 = 19;
const int line4 = 30;
const int line5 = 42;
const int line6 = 50;
const int line7 = 53;

//Create Multiple WIFI Object
WiFiMulti wifiMulti;
//WiFiClientSecure espCarCounter;  // Not using https
WiFiClient espCarCounter;

//const uint32_t connectTimeoutMs = 10000;
uint16_t connectTimeOutPerAP=5000;

/***** MQTT Setup Variables  *****/
PubSubClient mqtt_client(espCarCounter);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
//char mqtt_server[] = mqtt_Server;
//char mqtt_username[] = mqtt_UserName;
//char mqtt_password[] = mqtt_Password;
//const int mqtt_port = mqtt_Port;
bool mqtt_connected = false;
bool wifi_connected = false;
int wifi_connect_attempts = 5;

/***** GLOBAL VARIABLES *****/
unsigned int dayOfMonth;      // Current Calendar day
unsigned int lastDayOfMonth;  // Last calendar day used to reset days running
unsigned int currentHr12;     // Current Hour 12 Hour format
unsigned int currentHr24;     // Current Hour 24 Hour Format
unsigned int currentMin;      // Current Minute
unsigned int currentSec;      // Current Second
unsigned int daysRunning;     // Number of days the show is running.
unsigned int currentTimeMinute; // for converting clock time hh:mm to clock time min since midnight
int totalDailyCars; // total cars counted per day 24/7 Needed for debugging
int totalShowCars;  // total cars counted for durning show hours open (5:00 pm to 9:10 pm)

/***** TIME VARIABLES *****/
const unsigned long wifi_connectioncheckMillis = 5000; // check for connection every 5 sec
const unsigned long mqtt_connectionCheckMillis = 30000; // check for connection
unsigned long start_MqttMillis; // for Keep Alive Timer
unsigned long start_WiFiMillis; // for keep Alive Timer
unsigned long carCounterTimeout = 60000; // default time for car counter alarm in millis
unsigned long noCarTimer = 0;

//***** DAILY RESET FLAGS *****
bool flagDaysRunningReset = false;
bool flagMidnightReset = false;
bool flagDailyShowStartReset = false;
bool flagDailySummarySaved = false;
bool flagDailyShowSummarySaved = false;
bool flagHourlyReset = false;
bool showTime = false;
bool resetFlagsOnce = false;
// GAL 25-11-18: Track whether SD card is usable so we don't brick on failure
bool sdAvailable = false;

// ===========================
// Forward Declarations (needed for ordering)
// GAL 25-11-23.x
// ===========================
int determineSeasonYear();
void ensureSeasonFolderExists();
// ---- Forward declarations (needed because KeepMqttAlive uses these) ----
void saveDailyTotal();
void saveShowTotal();
void saveHourlyCounts();

void publishMQTT(const char *topic, const String &message, bool retainFlag);
void publishMQTT(const char *topic, const String &message);

// =====================================================
// SD Diagnostics Helper (Retained JSON)
// GAL 25-11-23: publish detailed SD status so failures are self-explaining
// step: "init", "open", "write", "ui_check", etc.
// file: path involved ("" if none)
// mode: "r", "w", "a" ("" if none)
// err : short error code/message ("" if OK)
// =====================================================
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

    publishMQTT(MQTT_PUB_SD_DIAG, String(diag), true);
}

// **********FILE NAMES FOR SD CARD *********
char seasonFolder[32] = "/CC/0000";   // default, updated at boot
File myFile;   //used to write files to SD Card
const String fileName1 = "/EnterTotal.txt"; // DailyTot.txt file to store daily counts in the event of a Failure
const String fileName2 = "/ShowTotal.txt";  // ShowTot.txt file to store season total counts
const String fileName3 = "/DayOfMonth.txt"; // DayOfMonth.txt file to store current day number
const String fileName4 = "/RunDays.txt"; // RunDays.txt file to store days since open
const String fileName5 = "/HourlyData.csv"; // HourlyData.csv Stores Daily Totals by Hour and total
const String fileName6 = "/EnterLog.csv"; // EnterLog.csv file to store all car counts for season (was MASTER.CSV)
const String fileName7 = "/ShowSummary.csv"; // Show summary of counts during show (5:00pm to 9:10pm)
const String fileName8 = "/data/index.html"; // data folder and index.html for serving files OTA
const String fileName9 = "/data/style.css"; // data folder and index.html for serving files OTA

/***** Arrays for Hourly Totals/Averages *****/
static unsigned int hourlyCount[24] = {0}; // Array for Daily total cars per hour
static float hourlyTemp[24] = {0.0};   // Array to store average temperatures for 24 hours

/***** Arrays Used to make display Pretty *****/
static char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// sync Time at REBOOT
void SetLocalTime()  {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time. Using Compiled Date");
      return;
  }
  //Following used for Debugging and can be commented out
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");
  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
 
  // Convert NTP time string to set RTC
  char timeStringBuff[50]; //50 chars should be enough
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.println(timeStringBuff);
  rtc.adjust(DateTime(timeStringBuff));
}

// WIFI Setup
// WIFI Setup (Primary-first with Backup)
// WIFI Setup (ESP32 primary-first with backup via WiFiMulti)
void setup_wifi()  {
    Serial.println("Connecting to WiFi");
    display.println("Connecting to WiFi..");
    display.display();

    WiFi.mode(WIFI_STA);

    // ----- Step 1: Scan once to see PRIMARY strength -----
    int n = WiFi.scanNetworks(false, true);
    int primaryRSSI = -127;
    bool primarySeen = false;

    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == secret_ssid_AP_1) {
            primaryRSSI = WiFi.RSSI(i);
            primarySeen = true;
            break;
        }
    }

    // ----- Step 2: If PRIMARY is seen and decent, connect to it directly -----
    if (primarySeen && primaryRSSI > -75) {
        WiFi.begin(secret_ssid_AP_1, secret_pass_AP_1);

        unsigned long startAttempt = millis();
        const unsigned long primaryTimeout = 8000;
        while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < primaryTimeout) {
            delay(200);
        }
    }

    // ----- Step 3: If not connected, fall back to WiFiMulti (PRIMARY+BACKUP) -----
    if (WiFi.status() != WL_CONNECTED) {
        while (wifiMulti.run(connectTimeOutPerAP) != WL_CONNECTED) {
            Serial.print(".");
            delay(200);
        }
    }

    // ----- Display / log final connection -----
    Serial.println("Connected to the WiFi network");
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.display();

    display.setCursor(0, line1);
    display.print("SSID: ");
    display.println(WiFi.SSID());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    IPAddress ip = WiFi.localIP();
    Serial.print("IP: ");
    Serial.println(ip);
    display.setCursor(0, line2);
    display.print("IP: ");
    display.println(ip);

    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    display.setCursor(0, line3);
    display.print("signal: ");
    display.print(rssi);
    display.println(" dBm");

    display.display();
    delay(1000);
}

  // END WiFi Setup

//WiFiMulti  actively switch back when MSB-CARCOUNTER returns
void preferPrimaryIfAvailable() {
if (WiFi.status() != WL_CONNECTED) return;
    if (WiFi.SSID() == secret_ssid_AP_1) return;   // already on PRIMARY (MSB-CARCOUNTER)

    // Scan for PRIMARY strength without wiping WiFi state
    int n = WiFi.scanNetworks(false, true);
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == secret_ssid_AP_1 && WiFi.RSSI(i) > -75) {
                // Primary is detected AND strong → switch back
                WiFi.disconnect(true);     // clear current connection
                delay(200);
                WiFi.begin(secret_ssid_AP_1, secret_pass_AP_1);
                return;
            }
        }
}


// Creates season folder based on current year
void ensureSeasonFolderExists() {
    int seasonYear = determineSeasonYear();
    snprintf(seasonFolder, sizeof(seasonFolder), "/CC/%04d", seasonYear);

    if (!SD.exists("/CC")) {
        SD.mkdir("/CC");
        publishMQTT(MQTT_DEBUG_LOG, "Created /CC directory");
    }

    if (!SD.exists(seasonFolder)) {
        SD.mkdir(seasonFolder);
        publishMQTT(MQTT_DEBUG_LOG, String("Created season folder: ") + seasonFolder);
    }

    publishMQTT(MQTT_DEBUG_LOG, String("Using season folder: ") + seasonFolder);
}

// BEGIN OTA SD Card File Operations
void listSDFiles(AsyncWebServerRequest *request) {
    String fileList = "Files in " + currentDirectory + ":\n";

    File root = SD.open(currentDirectory);
    if (!root || !root.isDirectory()) {
        request->send(500, "text/plain", "Failed to open directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        fileList += String(file.name()) + " (" + String(file.size()) + " bytes)\n";
        file = root.openNextFile();
    }

    request->send(200, "text/plain", fileList);
}

void downloadSDFile(AsyncWebServerRequest *request) {
    if (!request->hasParam("filename")) {
        request->send(400, "text/plain", "Filename is required");
        return;
    }

    String filename = currentDirectory + request->getParam("filename")->value();
    if (!SD.exists(filename)) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    // Add Content-Disposition header for proper filename handling
    AsyncWebServerResponse *response = request->beginResponse(SD, filename, "application/octet-stream");
    response->addHeader("Content-Disposition", "attachment; filename=\"" + String(request->getParam("filename")->value()) + "\"");

    request->send(response);
}

void uploadSDFile(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File uploadFile; // Keep track of the currently uploading file
    String fullPath = currentDirectory + "/" + filename; // Respect the current directory

    // Handle the start of the upload
    if (index == 0) {
        Serial.printf("Upload started: %s\n", fullPath.c_str());
        if (SD.exists(fullPath)) {
            SD.remove(fullPath); // Remove the file if it already exists
        }
        uploadFile = SD.open(fullPath, FILE_WRITE);
        if (!uploadFile) {
            Serial.printf("Failed to open file: %s\n", fullPath.c_str());
            request->send(500, "text/plain", "Failed to open file for writing");
            return;
        }
    }

    // Write data to the file
    if (uploadFile) {
        uploadFile.write(data, len);
    }

    // Handle the end of the upload
    if (final) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("Upload completed: %s\n", fullPath.c_str());
            request->send(200, "text/plain", "File uploaded successfully to " + currentDirectory);
        } else {
            Serial.printf("Upload failed: %s\n", fullPath.c_str());
            request->send(500, "text/plain", "Failed to write file");
        }
    }
}

void changeDirectory(AsyncWebServerRequest *request) {
    if (!request->hasParam("dir")) {
        request->send(400, "text/plain", "Directory name is required");
        return;
    }

    String newDirectory = request->getParam("dir")->value();
    if (newDirectory[0] != '/') {
        newDirectory = currentDirectory + "/" + newDirectory;
    }

    if (SD.exists(newDirectory) && SD.open(newDirectory).isDirectory()) {
        currentDirectory = newDirectory;
        request->send(200, "text/plain", "Changed directory to " + currentDirectory);
    } else {
        request->send(404, "text/plain", "Directory not found");
    }
}

void deleteSDFile(AsyncWebServerRequest *request) {
    if (!request->hasParam("filename")) {
        request->send(400, "text/plain", "Filename is required");
        return;
    }

    String fileName = request->getParam("filename")->value();
    String fullPath = currentDirectory + "/" + fileName; // Respect the current directory

    // Normalize the file path
    if (fullPath.startsWith("//")) {
        fullPath = fullPath.substring(1); // Remove redundant leading slashes
    }

    if (SD.exists(fullPath)) {
        if (SD.remove(fullPath)) {
            Serial.printf("File deleted: %s\n", fullPath.c_str());
            request->send(200, "text/plain", "File deleted successfully: " + fullPath);
        } else {
            Serial.printf("Failed to delete file: %s\n", fullPath.c_str());
            request->send(500, "text/plain", "Failed to delete file: " + fullPath);
        }
    } else {
        Serial.printf("File not found: %s\n", fullPath.c_str());
        request->send(404, "text/plain", "File not found: " + fullPath);
    }
}
//END OTA SD Card File Operations

// HTML Content served from /data/index.html and /data/style.css
void setupServer() {

    // ----------------------------
    // Root UI with SAFE fallback
    // ----------------------------
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (sdAvailable && SD.exists("/data/index.html")) {
            request->send(SD, "/data/index.html", "text/html");
            return;
        }

        // Fallback (never 500)
        request->send(200, "text/plain",
                      "Hi! This is The Car Counter. UI not found in /data.");
    });

    // ----------------------------
    // Serve CSS (fallback = 404)
    // ----------------------------
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (sdAvailable && SD.exists("/data/style.css")) {
            request->send(SD, "/data/style.css", "text/css");
            return;
        }

        request->send(404, "text/plain", "style.css not found in /data");
    });

    // ----------------------------
    // Serve Show Summary webpage
    // Put showSummary.html in /data/
    // ----------------------------
    server.on("/showSummary.html", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (sdAvailable && SD.exists("/data/showSummary.html")) {
            request->send(SD, "/data/showSummary.html", "text/html");
            return;
        }

        request->send(404, "text/plain", "showSummary.html not found in /data");
    });

    // ----------------------------
    // Seasonal ShowSummary.csv
    // /CC/YYYY/ShowSummary.csv
    // ----------------------------
    server.on("/ShowSummary.csv", HTTP_GET, [](AsyncWebServerRequest *request) {

        if (!sdAvailable) {
            request->send(503, "text/plain", "SD not available");
            return;
        }

        String fullPath = String(seasonFolder) + "/ShowSummary.csv";

        if (!SD.exists(fullPath)) {
            request->send(404, "text/plain",
                          "ShowSummary.csv not found in season folder");
            return;
        }

        AsyncWebServerResponse *response =
            request->beginResponse(SD, fullPath, "text/csv");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    // ----------------------------
    // File manager / SD routes
    // ----------------------------
    server.on("/listFiles", HTTP_GET, listSDFiles);
    server.on("/download", HTTP_GET, downloadSDFile);
    // Simple upload page for /uploadToData (so browser GET doesn't 500)
    server.on("/uploadToData", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html",
            "<!doctype html><html><body>"
            "<h3>Upload UI files to /data</h3>"
            "<form method='POST' action='/uploadToData' enctype='multipart/form-data'>"
            "<input type='file' name='file' multiple>"
            "<input type='submit' value='Upload to /data'>"
            "</form>"
            "<p>After upload, go back to <a href='/'>home</a>.</p>"
            "</body></html>"
        );
    });

    server.on("/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        uploadSDFile);

    // Handle file uploads to /data directory
    server.on("/uploadToData", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        [](AsyncWebServerRequest *request, String filename, size_t index,
           uint8_t *data, size_t len, bool final) {

            if (!sdAvailable) {
                request->send(503, "text/plain", "SD not available");
                return;
            }
            // GAL 25-11-23.x: guarantee /data exists
            if (!SD.exists("/data")) {
                SD.mkdir("/data");
            }
            String fullPath = "/data/" + filename;
            static File uploadFile;

            if (index == 0) { // First chunk
                if (SD.exists(fullPath)) SD.remove(fullPath);

                uploadFile = SD.open(fullPath, FILE_WRITE);
                if (!uploadFile) {
                    request->send(500, "text/plain",
                                  "Failed to open file for writing");
                    return;
                }
            }

            if (uploadFile) uploadFile.write(data, len);

            if (final) {
                if (uploadFile) uploadFile.close();
                request->send(200, "text/plain",
                              "File uploaded successfully to /data");
            }
        });

    server.on("/changeDirectory", HTTP_GET, changeDirectory);

    // ----------------------------
    // Elegant OTA
    // ----------------------------
    ElegantOTA.setID(THIS_MQTT_CLIENT);
    ElegantOTA.setFWVersion(FWVersion);
    ElegantOTA.setTitle(OTA_Title);

    ElegantOTA.begin(&server);
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);

    server.begin();
    Serial.println("HTTP server started");
}



/***** MQTT SECTION for Car Counter******/
// Queue for offline MQTT publishes
std::queue<String> publishQueue;

// GAL 25-11-21: Increased queue for multi-hour outages during busy nights
const size_t MAX_QUEUE = 3000;
const size_t MAX_FLUSH_PER_CALL = 150;   // prevents WDT resets



// Helper to encode retain flag into queue "topic|message|retain"
String encodeQueuedMessage(const char *topic, const String &msg, bool retainFlag) {
    return String(topic) + "|" + msg + "|" + (retainFlag ? "1" : "0");
}

// Car CounterOverload allowing explicit retain flag
void publishMQTT(const char *topic, const String &message, bool retainFlag) {
    if (mqtt_client.connected()) {
        mqtt_client.publish(topic, message.c_str(), retainFlag);
    } else {
        if (publishQueue.size() >= MAX_QUEUE) {
            publishQueue.pop();    // drop oldest
        }
        Serial.printf("MQTT offline. QUEUEING: %s -> %s (retain=%d)\n",
                      topic, message.c_str(), retainFlag);

        publishQueue.push(encodeQueuedMessage(topic, message, retainFlag));
    }
    start_MqttMillis = millis();
}

// Default wrapper = NO retain (for debug, HELLO, resets)
void publishMQTT(const char *topic, const String &message) {
    publishMQTT(topic, message, false);
}

// Publish queued messages (retain aware)
void publishQueuedMessages(size_t maxToFlush = MAX_FLUSH_PER_CALL) {
    size_t flushed = 0;

    while (!publishQueue.empty() &&
           mqtt_client.connected() &&
           flushed < maxToFlush) {

        String data = publishQueue.front();
        publishQueue.pop();

        int p1 = data.indexOf('|');
        int p2 = data.indexOf('|', p1 + 1);

        if (p1 != -1) {
            String topic   = data.substring(0, p1);
            String message = data.substring(p1 + 1, p2);
            bool retainFlag = (p2 != -1 && data.substring(p2 + 1) == "1");

            mqtt_client.publish(topic.c_str(), message.c_str(), retainFlag);
        }

        flushed++;
    }

    if (flushed > 0) {
        Serial.printf("MQTT Queue Flush: %u messages flushed, %u remain\n",
                      (unsigned)flushed, (unsigned)publishQueue.size());
    }
}

void publishDebugLog(const String &message) {
    publishMQTT(MQTT_DEBUG_LOG, message);   // never retained
}

// =====================================================
// GAL 25-11-22: MQTT Debug Event Publisher (remote console)
// Uses MQTT_DEBUG_LOG topic you already have
// =====================================================
void publishDebugEvent(const char* event, const String& details, bool retainFlag = false) {
    char buf[256];

    snprintf(buf, sizeof(buf),
        "{"
            "\"device\":\"%s\","
            "\"event\":\"%s\","
            "\"fw\":\"%s\","
            "\"time\":\"%s\","
            "\"details\":\"%s\""
        "}",
        THIS_MQTT_CLIENT,
        event,
        FWVersion,                     // <-- NO .c_str()
        bootTimestamp.c_str(),
        details.c_str()
    );

    publishMQTT(MQTT_DEBUG_LOG, String(buf), retainFlag);
}

// Used to publish current counts to update GATE Counter every 30 seconds if no car is counted
void KeepMqttAlive() {

    // ---- Heartbeat (retained, ONLY here to prevent show spam) ----
    // GAL 25-11-22.3: keep firmware visible for dashboards (retained overwrite)
    publishMQTT(MQTT_PUB_FW_VERSION, String(FWVersion), true);
    publishMQTT(
        MQTT_PUB_HEARTBEAT,
        String("{\"boot\":\"") + bootTimestamp +
        "\",\"now\":\"" + getRtcTimestamp() +
        "\",\"enter\":" + totalDailyCars +
        ",\"show\":" + totalShowCars +
        ",\"rssi\":" + WiFi.RSSI() +
        "}",
        true
    );

    // ---- Retained Temp/RH JSON ----
    char jsonPayload[100];
    snprintf(jsonPayload, sizeof(jsonPayload),
            "{\"tempF\": %.1f, \"humidity\": %.1f}", tempF, humidity);
// GAL 25-11-22.4: temp/RH publish removed from KeepMqttAlive; now on 10-min timer
// publishMQTT(MQTT_PUB_TEMP, String(jsonPayload), true);

    // ---- Retained core counts ----
    publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars), true);
    publishMQTT(MQTT_PUB_SHOWTOTAL,  String(totalShowCars),  true);

    // ---- WiFi Diagnostics (retained) ----
    publishMQTT(MQTT_PUB_RSSI, String(WiFi.RSSI()), true);
    publishMQTT(MQTT_PUB_SSID, WiFi.SSID(),        true);
    publishMQTT(MQTT_PUB_IP,   WiFi.localIP().toString(), true);

    // ---- Retained show window + date (for HA dashboards / GateCounter offset) ----
    // ---- Retained show window + date (for HA dashboards / GateCounter offset) ----
    char showStartDateBuf[11];
    snprintf(showStartDateBuf, sizeof(showStartDateBuf), "%04d-%02d-%02d",
            showStartY, showStartM, showStartD);
    publishMQTT(MQTT_PUB_SHOW_START_DATE, showStartDateBuf, true); // "YYYY-MM-DD"
    publishMQTT(MQTT_PUB_SHOW_START_MIN,  String(showStartMin), true);
    publishMQTT(MQTT_PUB_SHOW_END_MIN,    String(showEndMin), true);

    // ---- Hourly Snapshot (retained, unchanged) ----
    char buckets[180];
    snprintf(buckets, sizeof(buckets),
        "[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
        hourlyCount[0], hourlyCount[1], hourlyCount[2], hourlyCount[3],
        hourlyCount[4], hourlyCount[5], hourlyCount[6], hourlyCount[7],
        hourlyCount[8], hourlyCount[9], hourlyCount[10], hourlyCount[11],
        hourlyCount[12], hourlyCount[13], hourlyCount[14], hourlyCount[15],
        hourlyCount[16], hourlyCount[17], hourlyCount[18], hourlyCount[19],
        hourlyCount[20], hourlyCount[21], hourlyCount[22], hourlyCount[23]
    );
    publishMQTT(MQTT_PUB_HOURLY_JSON, String(buckets), true);

    // =====================================
    // Deferred SD writes (safe, non-hot-path)
    // =====================================
    if (sdAvailable) {

        if (pendingSaveDaily) {
            saveDailyTotal();
            pendingSaveDaily = false;
        }

        if (pendingSaveShow) {
            saveShowTotal();
            pendingSaveShow = false;
        }

        if (pendingSaveHourly) {
            saveHourlyCounts();
            pendingSaveHourly = false;
        }
    }
    start_MqttMillis = millis();
}

// =====================================================
// HELPERS SECTION
// Compute DaysRunning (RTClib) with Christmas Eve gap preserved
// - Matches historical DB semantics (no 12/24 show day)
// =====================================================
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

// =========================================================
// Determine Season Year (Show Start Year if known; else RTC)
// =========================================================
int determineSeasonYear() {
    if (showStartDateValid) return showStartY;
    DateTime nowDT = rtc.now();
    return nowDT.year();
}
// ===========================
// MQTT Reconnect Block
// ===========================
void callback(char* topic, byte* payload, unsigned int length);

//Connects to MQTT Server (single-server version)
void MQTTreconnect() {
    static unsigned long lastReconnectAttempt = 0; // Tracks the last reconnect attempt time
    const unsigned long reconnectInterval = 5000; // Time between reconnect attempts (5 seconds)

    // If the client is already connected, do nothing
    if (mqtt_client.connected()) {
        return;
    }

    // Check if enough time has passed since the last attempt
    if (millis() - lastReconnectAttempt < reconnectInterval) {
        return;
    }

    lastReconnectAttempt = millis(); // Update the last attempt time
    Serial.println("Attempting MQTT connection...");

    // Your existing single-server config
    mqtt_client.setServer(mqtt_Server, mqtt_Port);
    mqtt_client.setCallback(callback);  // required to receive messages

    // Create a unique client ID
    String clientId = THIS_MQTT_CLIENT;

    Serial.printf("Trying MQTT server: %s:%d\n", mqtt_Server, mqtt_Port);

    if (mqtt_client.connect(clientId.c_str(), mqtt_UserName, mqtt_Password)) {

        Serial.printf("Connected to MQTT server: %s\n", mqtt_Server);

        // Display connection status
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0,line5);
        display.println("MQTT Connect");
        display.display();

        Serial.println("connected!");
        Serial.println("Waiting for Car");

        // Once connected, publish retained online status
        publishMQTT(
            MQTT_PUB_HELLO,
            String("Car Counter ONLINE @ ") + bootTimestamp, true);
        // GAL 25-11-22: publish temp/RH as JSON (match HA templates)
        char jsonPayload[100];
        snprintf(jsonPayload, sizeof(jsonPayload),
                "{\"tempF\": %.1f, \"humidity\": %.1f}", tempF, humidity);
        publishMQTT(MQTT_PUB_TEMP, String(jsonPayload), true);
        publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars), true);
        publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars), true);

        // WiFi health immediately on connect (retained)
        publishMQTT(MQTT_PUB_RSSI, String(WiFi.RSSI()), true);
        publishMQTT(MQTT_PUB_SSID, String(WiFi.SSID()), true);
        publishMQTT(MQTT_PUB_IP,   WiFi.localIP().toString(), true);

        // GAL 25-11-22: retained online snapshot for remote debugging
        publishDebugEvent(
            "online",
            "ssid=" + WiFi.SSID() +
            " rssi=" + String(WiFi.RSSI()) +
            " ip=" + WiFi.localIP().toString(),
            true
        );

        // GAL 25-11-18: announce firmware version
// GAL 25-11-22.4: firmware publish retained for dashboards
        publishMQTT(MQTT_PUB_FW_VERSION, String(FWVersion), true); // ok non-retained

        // Subscribe to necessary topics
        mqtt_client.subscribe(MQTT_PUB_HELLO);
        mqtt_client.subscribe(MQTT_SUB_TOPIC1); // Reset Daily Count
        mqtt_client.subscribe(MQTT_SUB_TOPIC2); // Reset Show Count
        mqtt_client.subscribe(MQTT_SUB_TOPIC3); // Reset Day of Month
        mqtt_client.subscribe(MQTT_SUB_TOPIC4); // Reset Days Running
        mqtt_client.subscribe(MQTT_SUB_TOPIC5); // Update Car Counter Timeout
        mqtt_client.subscribe(MQTT_SUB_TOPIC6); // Update Sensor Wait Duration
        mqtt_client.subscribe(MQTT_SUB_SHOWSTART); // Set Show Start Time
        mqtt_client.subscribe(MQTT_SUB_SHOWSTARTTIME); // Set Show Start Time Detailed
        mqtt_client.subscribe(MQTT_SUB_SHOWENDTIME); // Set Show End Time Detailed

        // Log subscriptions
        Serial.println("Subscribed to MQTT topics.");
        publishDebugLog("MQTT connected and topics subscribed.");

        // Flush queued messages now that we're back online
        publishQueuedMessages();

        return;

    } else {
        Serial.printf("Failed to connect to MQTT server: %s (rc=%d)\n",
                      mqtt_Server, mqtt_client.state());

        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, line6);
        display.println("MQTT Error");
        display.display();
    }
}
/***** END MQTT SECTION *****/

void checkWiFiConnection() {

/* non-blocking WiFi and MQTT Connectivity Checks 
    First check if WiFi is connected */
    if (wifiMulti.run() == WL_CONNECTED) {
        /* If MQTT is not connected then Attempt MQTT Connection */
        if (!mqtt_client.connected()) {
            Serial.print("hour = ");
            Serial.println(currentHr12);
            Serial.println("Attempting MQTT Connection");
            MQTTreconnect();
            start_MqttMillis = millis();
        } else {
                //keep MQTT client connected when WiFi is connected
                mqtt_client.loop();
        }
    } else {
        // If WiFi if lost, then attemp non blocking WiFi Connection
        if ((millis() - start_WiFiMillis) > wifi_connectioncheckMillis) {
            setup_wifi();
            start_WiFiMillis = millis();
        }
    }    
}

// =========== GET SAVED SETUP FROM SD CARD ==========
// open DAILYTOT.txt to get initial dailyTotal value
void getDailyTotal() {
    if (!sdAvailable) {
        Serial.printf("getDailyTotal skipped - SD not available\n");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName1);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = SD.open(fullPath, FILE_READ);

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
    }
}


/** Get season total cars since show opened */
void getShowTotal() {
    if (!sdAvailable) {
        Serial.printf("getShowTotal skipped - SD not available\n");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName2);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = SD.open(fullPath, FILE_READ);
    if (myFile) {
        while (myFile.available()) {
            totalShowCars = myFile.parseInt();  // read total
        }
        myFile.close();

        Serial.print("Total Show cars from file = ");
        Serial.println(totalShowCars);

        publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
    } else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);
    }
}


// get the last calendar day used for reset daily counts
void getDayOfMonth() {
    if (!sdAvailable) {
        Serial.printf("getDayOfMonth skipped - SD not available\n");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName3);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = SD.open(fullPath, FILE_READ);
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
    }
}



// Days the show has been running
void getDaysRunning() {
    if (!sdAvailable) {
        Serial.printf("getDaysRunning skipped - SD not available\n");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName4);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = SD.open(fullPath, FILE_READ);
    if (myFile) {
        while (myFile.available()) {
            daysRunning = myFile.parseInt();  // read day number
        }
        myFile.close();

        Serial.print("Days Running = ");
        Serial.println(daysRunning);

        publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
    } else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);
    }
}



/** Get hourly car counts on reboot */
void getHourlyData() {
    if (!sdAvailable) {
        Serial.println("getHourlyData skipped - SD not available");
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
    File file = SD.open(fullPath, FILE_READ);
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



/***** UPDATE and SAVE TOTALS TO SD CARD *****/
/** Save the daily Total of cars counted */
void saveDailyTotal() {
    if (!sdAvailable) {
        Serial.println("saveDailyTotal skipped - SD not available");
        return;
    }

    // Build full seasonal path safely (fileName1 already starts with '/')
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fullPath = folder + fileName1;


    myFile = SD.open(fullPath, FILE_WRITE);  // overwrite same as before
    if (myFile) {
        myFile.print(totalDailyCars);
        myFile.close();
    } else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);
        publishMQTT(MQTT_DEBUG_LOG, "SD Write Failure: Unable to save daily total.");
    }
}

/* Save the grand total cars file for season  */
// GAL 25-11-23.2: add step-specific SD diagnostics for show total writes
void saveShowTotal() {

    if (!sdAvailable) {
        Serial.println("saveShowTotal skipped - SD not available");
        publishSdDiag_("open", "", "w", "skip_sdUnavailable");
        return;
    }
    
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName2);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    // ---- OPEN ----
    myFile = SD.open(fullPath, FILE_WRITE);  // overwrite, same as before
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


// Save the calendar day to file
void saveDayOfMonth() {
    if (!sdAvailable) {
        Serial.println("saveDayOfMonth skipped - SD not available");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName3);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = SD.open(fullPath, FILE_WRITE);  // overwrite, same as before
    if (myFile) {
        myFile.print(dayOfMonth);
        myFile.close();
    } else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);
        publishMQTT(MQTT_DEBUG_LOG, "SD Write Failure: Unable to save day of month.");
    }

    publishMQTT(MQTT_PUB_DAYOFMONTH, String(dayOfMonth));
}


/** Save number of days the show has been running */
void saveDaysRunning() {
    if (!sdAvailable) {
        Serial.println("saveDaysRunning skipped - SD not available");
        return;
    }

    // GAL 25-11-23.x: normalize path join to avoid double slashes
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fname = String(fileName4);
    if (!fname.startsWith("/")) fname = "/" + fname;

    String fullPath = folder + fname;

    myFile = SD.open(fullPath, FILE_WRITE);
    if (myFile) {
        myFile.print(daysRunning);
        myFile.close();
    } else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fullPath);
        publishMQTT(MQTT_DEBUG_LOG,
            "SD Write Failure: Unable to save days running.");
    }

    publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
}


// Save cars counted each hour in the event of a reboot
// Refactored saveHourlyCounts function (season-folder aware)
// Save cars counted each hour in the event of a reboot
// Refactored saveHourlyCounts function (season-folder aware)
void saveHourlyCounts() {
    if (!sdAvailable) {
        Serial.println("saveHourlyCounts skipped - SD not available");
        return;
    }

    DateTime now = rtc.now();
    char dateBuffer[13];
    snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d",
             now.year(), now.month(), now.day());

    int currentHour = now.hour(); // Get the current hour (0-23)

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
    File file = SD.open(fullPath, FILE_READ);
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
                char topic[60];
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

        char topic[60];
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
    file = SD.open(fullPath, FILE_WRITE);
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


// Save and Publish Show Totals
void saveDailyShowSummary() {
    if (!sdAvailable) {
        Serial.println("saveDailyShowSummary skipped - SD not available");
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
    File summaryFile = SD.open(fullPath, FILE_APPEND);
    if (!summaryFile) {
        Serial.print("Failed to open daily show summary file: ");
        Serial.println(fullPath);
        publishMQTT(MQTT_DEBUG_LOG, "Failed to open daily show summary file: " + fullPath);
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

    // Publish show summary data to MQTT
    publishMQTT(MQTT_PUB_SUMMARY, String("Date: ") + dateBuffer +
                                        ", DaysRunning: " + daysRunning +
                                        ", Before5: " + totalBefore5 +
                                        ", 6PM: " + cumulative6PM +
                                        ", 7PM: " + cumulative7PM +
                                        ", 8PM: " + cumulative8PM +
                                        ", 9PM: " + cumulative9PM +
                                        ", ShowTotal: " + totalShowCars +
                                        ", ShowAvgTemp: " + String(showAverageTemp, 1));

    Serial.printf("Daily show summary written: %s, DaysRunning: %d, Before5: %d, 6PM: %d, 7PM: %d, 8PM: %d, 9PM: %d, ShowTotal: %d, Avg Temp: %.1f°F.\n",
                  dateBuffer, daysRunning, totalBefore5, cumulative6PM, cumulative7PM,
                  cumulative8PM, cumulative9PM, totalShowCars, showAverageTemp);
}

void getSavedValuesOnReboot() {
    // GAL 25-11-18: If SD is offline, start fresh but keep running
    if (!sdAvailable) {
        totalDailyCars = 0;
        totalShowCars  = 0;
        daysRunning    = 0;
        memset(hourlyCount, 0, sizeof(hourlyCount));
        Serial.println("getSavedValuesOnReboot: SD not available, counters reset to 0.");
        return;
    }

    DateTime now = rtc.now();

    // Load last day of month from SD
    getDayOfMonth();

    // Did we cross a day boundary?
    if (now.day() != lastDayOfMonth) {

        // Update dayOfMonth and save
        dayOfMonth = now.day();
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
        if (showStartDateValid && rtcReady && now.year() >= 2024) {
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

        Serial.println("ESP32 reboot detected on same day. Reloading saved counts.");
        publishMQTT(MQTT_DEBUG_LOG, "Rebooted: Counts reloaded for same day.");
    }
}

/***** END OF DATA STORAGE & RETRIEVAL OPS *****/

/*** MQTT CALLBACK TOPICS for Received messages ****/
void callback(char* topic, byte* payload, unsigned int length) {

    // Copy payload into a safe, null-terminated buffer
    char message[length + 1];
    strncpy(message, (char*)payload, length);
    message[length] = '\0';

    if (strcmp(topic, MQTT_SUB_TOPIC1) == 0)  {
        /* Topic used to manually reset Enter Daily Cars */
        totalDailyCars = atoi(message);
        saveDailyTotal();
        Serial.println(F(" Car Counter Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Daily Total Updated");

    } else if (strcmp(topic, MQTT_SUB_TOPIC2) == 0)  {
        /* Topic used to manually reset Total Show Cars */
        totalShowCars = atoi(message);
        saveShowTotal();
        Serial.println(F(" Show Counter Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Show Counter Updated");

    } else if (strcmp(topic, MQTT_SUB_TOPIC3) == 0)  {
        /* Topic used to manually reset Calendar Day */
        dayOfMonth = atoi(message);
        saveDayOfMonth();
        Serial.println(F(" Calendar Day of Month Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Calendar Day Updated");

    } else if (strcmp(topic, MQTT_SUB_TOPIC4) == 0)  {
        /* Topic used to manually reset Days Running */
        daysRunning = atoi(message);
        saveDaysRunning();
        Serial.println(F(" Days Running Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Days Running Updated");

    } else if (strcmp(topic, MQTT_SUB_TOPIC5) == 0)  {
        // Topic used to change car counter timeout (ms)
        unsigned long v = strtoul(message, nullptr, 10);
        if (v < 5000) v = 5000;        // floor 5s
        if (v > 600000) v = 600000;    // ceiling 10 min
        carCounterTimeout = v;

        Serial.println(F(" Counter Alarm Timer Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Car Counter Timeout Updated");

    } else if (strcmp(topic, MQTT_SUB_TOPIC6) == 0)  {
        // Topic used to change beam stuck-high duration (ms)
        unsigned long v = strtoul(message, nullptr, 10);
        if (v < 5000) v = 5000;
        if (v > 600000) v = 600000;
        waitDuration = v;

        Serial.println(F(" Beam Sensor Duration Updated"));
        publishMQTT(MQTT_DEBUG_LOG, "Beam Sensor Duration Updated");

    } else if (strcmp(topic, MQTT_SUB_SHOWSTART) == 0) {
        // Set Show Start Date (YYYY-MM-DD)
        int y, m, d;

        if (sscanf(message, "%d-%d-%d", &y, &m, &d) == 3) {

            // ---- guard against junk / uninitialized dates ----
            bool valid =
                (y >= 2020 && y <= 2100) &&
                (m >= 1 && m <= 12) &&
                (d >= 1 && d <= 31);

            if (!valid) {
                publishMQTT(MQTT_DEBUG_LOG,
                    String("Invalid ShowStartDate payload (range): ") + message);
                return;   // don't overwrite stored values
            }

            // ---- idempotent guard: same date => do nothing ----
            if (showStartDateValid &&
                y == showStartY && m == showStartM && d == showStartD) {
                publishMQTT(MQTT_DEBUG_LOG,
                    String("ShowStartDate unchanged, ignoring: ") + message);
                return;   // prevents side effects from HA reasserts
            }

            // ---- accept NEW date ----
            showStartY = y;
            showStartM = m;
            showStartD = d;
            showStartDateValid = true;

            // update SD season folder only on real change
            ensureSeasonFolderExists();
            publishMQTT(MQTT_DEBUG_LOG,
                String("SD season folder updated to: ") + seasonFolder);

            // recompute derived values ONLY (no resets here)
            daysRunning = computeDaysRunning();
            saveDaysRunning();   // publishes retained DaysRunning
            publishMQTT(MQTT_DEBUG_LOG,
                "ShowStartDate received, DaysRunning recomputed: " + String(daysRunning));

        } else {
            publishMQTT(MQTT_DEBUG_LOG,
                String("Invalid ShowStartDate payload (parse): ") + message);
        }

    } else if (strcmp(topic, MQTT_SUB_SHOWSTARTTIME) == 0) {
        // Update show start time (minutes since midnight)
        // Expect hh:mm
        int hh = 0, mm = 0;
        if (sscanf(message, "%d:%d", &hh, &mm) == 2) {
            showStartMin = hh * 60 + mm;

            Serial.printf("Show Start Time Updated → %02d:%02d (%d minutes)\n",
                          hh, mm, showStartMin);

            publishMQTT(MQTT_DEBUG_LOG,
                        String("Show Start Time Updated to ") + message);
        } else {
            publishMQTT(MQTT_DEBUG_LOG,
                        String("Invalid ShowStartTime payload: ") + message);
        }

    } else if (strcmp(topic, MQTT_SUB_SHOWENDTIME) == 0) {
        // Update show end time (minutes since midnight)
        // Expect hh:mm
        int hh = 0, mm = 0;
        if (sscanf(message, "%d:%d", &hh, &mm) == 2) {
            showEndMin = hh * 60 + mm;

            Serial.printf("Show End Time Updated → %02d:%02d (%d minutes)\n",
                          hh, mm, showEndMin);

            publishMQTT(MQTT_DEBUG_LOG,
                        String("Show End Time Updated to ") + message);
        } else {
            publishMQTT(MQTT_DEBUG_LOG,
                        String("Invalid ShowEndTime payload: ") + message);
        }
    }
}
/***** END OF CALLBACK TOPICS *****/


/***** IDLE STUFF  *****/
// Flash an alternating pattern on the arches (called if a car hasn't been detected for over 30 seconds)
void playPattern() {
    unsigned long currentMillis = millis();
    
    switch (currentPatternState) {
        case INITIAL_OFF:
            if (currentMillis - patternModeMillis >= 500) {
                digitalWrite(redArchPin, HIGH);
                digitalWrite(greenArchPin, HIGH);
                currentPatternState = RED_ON;
                patternModeMillis = currentMillis;
            }
            break;
        
        case RED_ON:
            if (currentMillis - patternModeMillis >= 500) {
                digitalWrite(redArchPin, LOW);
                digitalWrite(greenArchPin, HIGH);
                currentPatternState = GREEN_ON;
                patternModeMillis = currentMillis;
            }
            break;
        
        case GREEN_ON:
            if (currentMillis - patternModeMillis >= 500) {
                digitalWrite(redArchPin, HIGH);
                digitalWrite(greenArchPin, LOW);
                currentPatternState = BOTH_ON;
                patternModeMillis = currentMillis;
            }
            break;
        
        case BOTH_ON:
            if (currentMillis - patternModeMillis >= 500) {
                digitalWrite(redArchPin, LOW);
                digitalWrite(greenArchPin, LOW);
                currentPatternState = BOTH_OFF;
                patternModeMillis = currentMillis;
            }
            break;
        
        case BOTH_OFF:
            if (currentMillis - patternModeMillis >= 500) {
                digitalWrite(redArchPin, HIGH);
                digitalWrite(greenArchPin, HIGH);
                currentPatternState = RED_OFF_GREEN_ON;
                patternModeMillis = currentMillis;
            }
            break;
        
        case RED_OFF_GREEN_ON:
            if (currentMillis - patternModeMillis >= 500) {
                digitalWrite(redArchPin, HIGH);
                digitalWrite(greenArchPin, LOW);
                currentPatternState = GREEN_OFF_RED_ON;
                patternModeMillis = currentMillis;
            }
            break;
        
        case GREEN_OFF_RED_ON:
            if (currentMillis - patternModeMillis >= 500) {
                digitalWrite(redArchPin, LOW);
                digitalWrite(greenArchPin, HIGH);
                currentPatternState = INITIAL_OFF;
                patternModeMillis = currentMillis;
            }
            break;
    }
}

// Average Temperature each hour
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

// Car Counted, increment the counter by 1 and append to the Enterlog.csv log file on the SD card
void countTheCar() {
    /* COUNT SUCCESS */

    noCarTimer = millis();

    /*------Append to log file*/
    DateTime now = rtc.now();
    char buf2[] = "YYYY-MM-DD hh:mm:ss";
    Serial.print(now.toString(buf2));
    Serial.print(", Temp = ");
    Serial.print(tempF);
    Serial.print(", ");
    // increase Count for Every car going through car counter regardless of time
    totalDailyCars ++;   
    // Increment hourly car count
    int currentHour = now.hour();
    hourlyCount[currentHour]++;
    pendingSaveHourly = true;   // defer SD hourly write
    //saveDailyTotal(); // DEFERREDUpdate Daily Total on SD Card to retain numbers with reboot
    pendingSaveDaily = true;   // <-- new global flag we will handle next step
    //saveHourlyCounts(); // removed for SD Card hot-path safety

    // Construct the MQTT topic dynamically
    char topic[60];
    snprintf(topic, sizeof(topic), "%s%02d", MQTT_PUB_CARS_HOURLY, currentHour);
    // Publish current hour's data to MQTT
    publishMQTT(topic, String(hourlyCount[currentHour]));


    // increase Show Count only when show is open
    if (showTime == true) {
        totalShowCars ++;  // increase Show Count only when show is open
        //saveShowTotal(); // update show total count in event of power failure during show hours
        pendingSaveShow = true;  // <-- new global flag we will handle next step
    }
    Serial.print(totalDailyCars) ;  
    // open file for writing Car Data
    myFile = SD.open(fileName6, FILE_APPEND); //Enterlog.csv
    if (myFile) {
        myFile.print(now.toString(buf2));
        myFile.print(", ");
        myFile.print (timeToPassMS) ; 
        myFile.print(", 1 , "); 
        myFile.print (totalDailyCars) ; 
        myFile.print(", ");
        myFile.println(tempF);
        myFile.close();
        Serial.print(F(" Car "));
        Serial.print(totalDailyCars);
        Serial.println(F(" Logged to SD Card."));
        char jsonPayload[64];
        snprintf(jsonPayload, sizeof(jsonPayload),
                "{\"tempF\": %.1f, \"humidity\": %.1f}",
                tempF, humidity);

        publishMQTT(MQTT_PUB_TEMP, String(jsonPayload), true);
        publishMQTT(MQTT_PUB_TIME, getRtcTimestamp());
        publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars));
        start_MqttMillis = millis();
    } else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fileName6);
    }
} /* END countTheCar*/

/** CarCounter State Machine to Detect and Count Cars */
void detectCar() {
    unsigned long currentMillis = millis();
    bool rawFirstBeamState = digitalRead(firstBeamPin);
    bool rawSecondBeamState = digitalRead(secondBeamPin);

    // Debounce first beam sensor
    if (rawFirstBeamState != stableFirstBeamState) {
        if (currentMillis - lastFirstBeamChangeTime > debounceDelay) {
            stableFirstBeamState = rawFirstBeamState;
            lastFirstBeamChangeTime = currentMillis;
            // Publish the state change only when it's stable
            publishMQTT(MQTT_FIRST_BEAM_SENSOR_STATE, String(stableFirstBeamState));
        }
    } else {
        lastFirstBeamChangeTime = currentMillis;
    }

    // Debounce second beam sensor
    if (rawSecondBeamState != stableSecondBeamState) {
        if (currentMillis - lastSecondBeamChangeTime > debounceDelay) {
            stableSecondBeamState = rawSecondBeamState;
            lastSecondBeamChangeTime = currentMillis;
            // Publish the state change only when it's stable
            publishMQTT(MQTT_SECOND_BEAM_SENSOR_STATE, String(stableSecondBeamState));
        }
    } else {
        lastSecondBeamChangeTime = currentMillis;
    }

    // Use the stable beam states instead of the raw readings
    firstBeamState = stableFirstBeamState;
    secondBeamState = stableSecondBeamState;

    // Handle detection states using the stable states
    switch (currentCarDetectState) {
        case WAITING_FOR_CAR:
            if (firstBeamState == 1) {
                firstBeamTripTime = currentMillis; // Set the time when the first beam is tripped
                currentCarDetectState = FIRST_BEAM_HIGH;
                publishMQTT(MQTT_COUNTER_LOG, "First Beam High");
                digitalWrite(greenArchPin, HIGH); // Turn on green arch
            }
            break;

        case FIRST_BEAM_HIGH:
            if (secondBeamState == 1) {
                // Both beams have been tripped, measure time for validation
                unsigned long timeBeamsHigh = currentMillis - firstBeamTripTime;
                // Only move to BOTH_BEAMS_BROKEN if the time exceeds minimum activation duration
                if (timeBeamsHigh >= minActivationDuration && timeBeamsHigh >= waitDuration) {
                    bothBeamsHighTime = currentMillis; // Set the time when both beams are confirmed to be high
                    carPresentFlag = true;
                    currentCarDetectState = BOTH_BEAMS_HIGH;
                    publishMQTT(MQTT_COUNTER_LOG, "State Changed Both Beams High");
                    digitalWrite(greenArchPin, LOW);  // Turn off green arch
                    digitalWrite(redArchPin, HIGH);   // Turn on red arch
                 }
                
            } else if (firstBeamState == 0) {
                // If the first beam is cleared before the second beam is triggered, reset
                currentCarDetectState = WAITING_FOR_CAR;
                publishMQTT(MQTT_COUNTER_LOG, "1st Beam Low, Changed Waiting for Car");
                digitalWrite(greenArchPin, HIGH); // Turn on green arch
            }
            break;

            case BOTH_BEAMS_HIGH: {
                static bool stuckAlarmSent = false;

                if (currentMillis - firstBeamTripTime >= carCounterTimeout) {
                    if (!stuckAlarmSent) {
                        // Alarm event (non-retained)
                        publishMQTT(MQTT_PUB_ALARM, "ALARM_CAR_STUCK", false);

                        // Optional: keep legacy text on HELLO during transition
                        // publishMQTT(MQTT_DEBUG_LOG, "Check Car Counter!", false);

                        publishMQTT(MQTT_COUNTER_LOG, "Alarm: Car stuck at car counter", false);
                        stuckAlarmSent = true;
                    }
                }

                // Clear alarm latch when beam clears
                if (secondBeamState == 0) {
                    stuckAlarmSent = false;
                }

                if (secondBeamState == 0 && carPresentFlag) {
                    currentCarDetectState = CAR_DETECTED;
                    publishMQTT(MQTT_COUNTER_LOG, "Changed state to Car Detected", false);
                }
                break;
            }

        case CAR_DETECTED:
            if (carPresentFlag) {
                countTheCar(); // Count the car and update files
                unsigned long timeToPassMS = currentMillis - firstBeamTripTime;
                publishMQTT(MQTT_PUB_TTP, String(timeToPassMS));
                publishMQTT(MQTT_COUNTER_LOG, "Car Counted Successfully!!");
                carPresentFlag = false;

                // Update lights on successful detection
                digitalWrite(redArchPin, LOW);  // Turn off red arch
                digitalWrite(greenArchPin, HIGH); // Turn on green arch

                // Record the time of detection for calculating time between cars
                unsigned long timeBetweenCars = currentMillis - lastCarDetectedMillis;
                publishMQTT(MQTT_PUB_TIMEBETWEENCARS, String(timeBetweenCars));
                lastCarDetectedMillis = currentMillis;

                // Reset to waiting for a new car
                currentCarDetectState = WAITING_FOR_CAR;
                publishMQTT(MQTT_COUNTER_LOG, "Idle-Waiting For Car");
            }
            break;
    }
}
// END CarCounter CAR DETECTION

// CHECK AND CREATE FILES on SD Card and WRITE HEADERS if Needed
// GAL 25-11-18: Make file creation non-fatal if SD has issues
// CHECK AND CREATE FILES on SD Card and WRITE HEADERS if Needed
// GAL 25-11-18: Make file creation non-fatal if SD has issues
// GAL 25-11-23.2: Seasonal files live in season folder; /data/* files stay at SD root

void checkAndCreateFile(const String &fileName, const String &header = "") {
    if (!sdAvailable) {
        Serial.printf("checkAndCreateFile skipped (%s) - SD not available\n", fileName.c_str());
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
            if (!SD.mkdir(fullPath)) {
                Serial.printf("Failed to create directory %s\n", fullPath.c_str());
            } else {
                Serial.printf("Directory %s created successfully\n", fullPath.c_str());
            }

        } else { // Create file
            File file = SD.open(fullPath, FILE_WRITE);
            if (!file) {
                Serial.printf("Failed to create file %s\n", fullPath.c_str());
            } else {
                if (!header.isEmpty()) {
                    file.print(header);
                }
                file.close();
                Serial.printf("File %s created successfully\n", fullPath.c_str());
            }
        }
    }
}


// GAL 25-11-23.x: Hourly file now created/initialized in season folder (/CC/YYYY/)
void createAndInitializeHourlyFile(const String &fileName) {

    // Build full seasonal path safely (fileName already starts with '/')
    String folder = String(seasonFolder);
    if (folder.endsWith("/")) folder.remove(folder.length() - 1);

    String fullPath = folder + fileName;

    if (!SD.exists(fullPath)) {
        Serial.printf("%s not found. Creating...\n", fullPath.c_str());

        File file = SD.open(fullPath, FILE_WRITE);
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


/** Initialize microSD card */
// GAL 25-11-18: Make SD init non-blocking; device runs even if SD is missing
// GAL 25-11-23.2: Add boot-time SD path checks + write/readback test + retained sdDiag
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
        publishMQTT(MQTT_DEBUG_LOG, "SD FAIL: mount failed", false);

        publishSdDiag_("init", "", "", "mount_fail");
        return;
    }


    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        publishMQTT(MQTT_PUB_SD_STATUS, "FAIL_NO_CARD", true);
        publishMQTT(MQTT_DEBUG_LOG, "SD FAIL: no card", false);

        publishSdDiag_("init", "", "", "no_card");
        return;
    }

    // GAL 25-11-23.2: SD.cardSize() can return 0 on ESP32 SPI; use totalBytes()
    uint64_t cardSizeMB = SD.totalBytes() / (1024ULL * 1024ULL);
    Serial.printf("SD Card Size: %lluMB\n", cardSizeMB);

    // ---- Validate critical folders/files ----
    bool dataDirExists = SD.exists("/data");
    bool uiIndexExists = SD.exists("/data/index.html");   // if your UI entry file differs, we’ll adjust later

    // Ensure /CC exists
    bool ccDirExists = SD.exists("/CC");
    if (!ccDirExists) {
        ccDirExists = SD.mkdir("/CC");
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
        seasonDirExists = SD.mkdir(seasonFolder);
        if (seasonDirExists) {
            publishMQTT(MQTT_DEBUG_LOG,
                String("Created season folder: ") + seasonFolder, false);
        } else {
            publishMQTT(MQTT_DEBUG_LOG,
                String("SD FAIL: unable to create season folder: ") + seasonFolder, false);
        }
    }

    // ---- Lightweight write/readback test ----
    bool writeTestOK = false;
    const char* testFile = "/sd_test.txt";

    File tf = SD.open(testFile, FILE_WRITE);
    if (tf) {
        size_t n = tf.print("test\n");
        tf.flush();
        tf.close();

        if (n > 0) {
            File rf = SD.open(testFile, FILE_READ);
            if (rf) {
                String s = rf.readStringUntil('\n');
                rf.close();
                writeTestOK = (s == "test");
            }
        }

        SD.remove(testFile);
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
        publishMQTT(MQTT_DEBUG_LOG,
            String("SD OK: ") + String(cardSizeMB) + "MB", false);
    } else {
        publishMQTT(MQTT_PUB_SD_STATUS, "FAIL_DEGRADED", true);
        publishMQTT(MQTT_DEBUG_LOG, "SD FAIL: degraded (see sdDiag)", false);
    }

    // Retained structured diag
    char diag2[220];
    snprintf(diag2, sizeof(diag2),
        "{\"sizeMB\":%llu,\"dataDir\":%s,\"uiIndex\":%s,\"ccDir\":%s,\"seasonDir\":%s,\"writeTest\":%s}",
        cardSizeMB,
        dataDirExists ? "true" : "false",
        uiIndexExists ? "true" : "false",
        ccDirExists ? "true" : "false",
        seasonDirExists ? "true" : "false",
        writeTestOK ? "true" : "false"
    );
    publishMQTT(MQTT_PUB_SD_DIAG, String(diag2), true);
}



//Car Counter Temperature and Humidity Reading
void readTempandRH() {
    static unsigned long lastDHTReadMillis = 0;    // Last time temperature was read
    static unsigned long lastDHTPrintMillis = 0;   // Last time temperature was printed
    const unsigned long dhtReadInterval = 10000;  // 10 seconds interval for reading temp
    const unsigned long dhtPrintInterval = 600000; // 10 minutes interval for printing temp
    static bool tempOutOfRangeReported = false;

    unsigned long currentMillis = millis();

    // Check if it's time to read the sensor
    if (currentMillis - lastDHTReadMillis >= dhtReadInterval) {
        lastDHTReadMillis = currentMillis;

        // Read temperature and humidity
        humidity = dht.readHumidity();
        tempF = dht.readTemperature(true); // Read Fahrenheit directly

        // Check if the readings are valid
        if (isnan(tempF) || isnan(humidity)) {
            Serial.println("Failed to read from DHT sensor!");
            publishDebugLog("DHT sensor reading failed.");
            tempF = -999;  // Use a sentinel value to indicate failure
            humidity = -999;
            return; // Exit function if the readings are invalid
        }
        // Check for temperature out of range
        if (tempF < -40 || tempF > 120) {
            if (!tempOutOfRangeReported) {
                // Publish only if not already reported
                Serial.println("Temperature out of range!");
                publishDebugLog("DHT temperature out of range: " + String(tempF));
                tempOutOfRangeReported = true; // Set flag to prevent duplicate reporting
            }
            tempF = -999; // Set to sentinel value for out-of-range condition
        } else {
            // Reset the flag if temperature is back in range
            if (tempOutOfRangeReported) {
                Serial.println("Temperature back in range.");
                tempOutOfRangeReported = false;
            }

            // Publish the temperature and humidity as JSON to MQTT
            char jsonPayload[100];
            snprintf(jsonPayload, sizeof(jsonPayload), "{\"tempF\": %.1f, \"humidity\": %.1f}", tempF, humidity);
// GAL 25-11-22.4: temp/RH publish moved to 10-min timer in loop to reduce spam
// publishMQTT(MQTT_PUB_TEMP, String(jsonPayload));

            // Forward valid readings to the hourly average system
            averageHourlyTemp(); // Ensure the reading is processed for summaries
        }

        // Check if it's time to print the readings
        if (currentMillis - lastDHTPrintMillis >= dhtPrintInterval) {
            lastDHTPrintMillis = currentMillis;

            // Print temperature and humidity readings
            if (tempF != -999 && humidity != -999) {
                Serial.printf("Temperature: %.1f °F, Humidity: %.1f %%\n", tempF, humidity);
            } else {
                Serial.println("Temperature/Humidity data invalid. Check sensor.");
            }
        }
    }   
}

/** Resets the hourly count array at midnight */
void resetHourlyCounts() {
    for (int i = 0; i < 24; i++) {
        hourlyCount[i] = 0; // Reset the hourly counts
    }
    // Log the reset
    Serial.println("Hourly counts have been reset.");
    publishMQTT(MQTT_DEBUG_LOG, "Hourly counts reset for the new day.");
}

/* Resets counts at Start of Show and Midnight */
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
    if (minutesNow == showEndMin && !flagDailyShowSummarySaved) {
        saveDailyShowSummary();
        publishMQTT(MQTT_DEBUG_LOG, "Daily Show Summary Saved (showEndMin)");
        flagDailyShowSummarySaved = true;
    }

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



// Update OLED Display while running
void updateDisplay() {
    DateTime now = rtc.now();
    //float tempF = ((rtc.getTemperature() * 9 / 5) + 32); // Get temperature in Fahrenheit
    int currentHr24 = now.hour();
    int currentHr12 = currentHr24 > 12 ? currentHr24 - 12 : (currentHr24 == 0 ? 12 : currentHr24);
    const char* ampm = currentHr24 < 12 ? "AM" : "PM";

    // Clear display and set formatting
    display.clearDisplay();
    display.setTextColor(WHITE);
    //display.setFont(&FreeSans12pt7b);

    // Line 1: Date and Day of the Week
    display.setTextSize(1);
    display.setCursor(0, line1);
    display.printf("%s %s %02d, %04d", days[now.dayOfTheWeek()], months[now.month() - 1], now.day(), now.year());

    // Line 2: Time and Temperature
    display.setCursor(0, line2);
    display.printf("%02d:%02d:%02d %s  %d F", currentHr12, now.minute(), now.second(), ampm, (int)tempF);

    // Line 3: Days Running and Show Total
    display.setCursor(0, line3);
    display.printf("Day %d  Total: %d", daysRunning, totalShowCars);

    // Line 4: Total Daily Cars
    display.setTextSize(2);
    display.setCursor(0, 40);
    display.printf("Cars: %d", totalDailyCars);

    // Write to the display
    display.display();
}

/******  BEGIN SETUP ******/
void setup() {
    Serial.begin(115200);
    ElegantOTA.setAutoReboot(true);
    ElegantOTA.setFilesystemMode(true);
    Serial.println("Starting Car Counter...");

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
    wifiMulti.addAP(secret_ssid_AP_1, secret_pass_AP_1);
    wifiMulti.addAP(secret_ssid_AP_2, secret_pass_AP_2);
    wifiMulti.addAP(secret_ssid_AP_3, secret_pass_AP_3);
    wifiMulti.addAP(secret_ssid_AP_4, secret_pass_AP_4);
    wifiMulti.addAP(secret_ssid_AP_5, secret_pass_AP_5);
    setup_wifi();

    // Initialize MQTT
    //mqtt_client.setServer(mqtt_server, mqtt_port);
    //mqtt_client.setCallback(callback);



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
    SetLocalTime();
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
    MQTTreconnect(); // Ensure MQTT is connected

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
    readTempandRH();

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
            SD.mkdir("/data");
            publishMQTT(MQTT_DEBUG_LOG, "Created /data directory", false);
        }

        // Check and create Required Data files
        checkAndCreateFile(fileName1);
        checkAndCreateFile(fileName2);
        checkAndCreateFile(fileName3);
        checkAndCreateFile(fileName4);
        checkAndCreateFile(fileName6, "Date Time,TimeToPass,Car#,TotalDailyCars,Temp");
        checkAndCreateFile(fileName7, "Date,DaysRunning,Before5,6PM,7PM,8PM,9PM,ShowTotal,DailyAvgTemp");
        checkAndCreateFile(fileName8);
        checkAndCreateFile(fileName9);

        // Required Hourly Data Files
        createAndInitializeHourlyFile(fileName5);
     
    } else {
        publishMQTT(MQTT_DEBUG_LOG, "SD init failed - skipping SD file setup", false);
    }


    // Initialize Server
    setupServer();

    //on reboot, get totals saved on SD Card
    getSavedValuesOnReboot();  // Update/reset counts based on reboot day

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
} /***** END SETUP ******/

void loop() {

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

    DateTime now = rtc.now();

    currentTimeMinute = now.hour()*60 + now.minute(); // convert time to minutes since midnight

    // Date guard: only count show cars on/after showStartY/M/D
    bool dateOk = false;
    if (showStartDateValid && rtcReady) {
        DateTime today(now.year(), now.month(), now.day(), 0, 0, 0);
        DateTime start(showStartY, showStartM, showStartD, 0, 0, 0);
        dateOk = (today.unixtime() >= start.unixtime());
    }

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
/***** Repeat Loop *****/