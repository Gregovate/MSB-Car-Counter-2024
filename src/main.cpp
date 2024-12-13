/*
Car counter Interface to MQTT by Greg Liebig gliebig@sheboyganlights.org
Initial Build 12/5/2023 12:15 pm
Changed time format YYYY-MM-DD hh:mm:ss 12/13/23

Changelog
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
SPI Pins
D5 - CS
D18 - CLK
D19 - MISO
D23 - MOSI

*/
#include <Arduino.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"
#include "NTPClient.h"
//#include "WIFI.h"
//#include "WiFiClientSecure.h"
#include "WiFiMulti.h"
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
#include <queue>  // Include queue for storing messages

// ******************** CONSTANTS *******************
#define FWVersion "24.12.11.2" // Firmware Version
#define OTA_Title "Car Counter" // OTA Title
#define DS3231_I2C_ADDRESS 0x68 // Real Time Clock
#define firstBeamPin 33
#define secondBeamPin 32
#define redArchPin 25
#define greenArchPin 26
#define PIN_SPI_CS 5 // The ESP32 pin GPIO5
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// #define MQTT_KEEPALIVE 30 //removed 10/16/24

unsigned int waitDuration = 950; // minimum millis for secondBeam to be broken needed to detect a car
unsigned int showStartTime = 17*60; // Show (counting) starts at 5:00 pm
unsigned int showEndTime =  21*60 + 10;  // Show (counting) ends at 9:10 pm 
// **************************************************

//***** MQTT DEFINITIONS *****/
#define THIS_MQTT_CLIENT "espCarCounter" // Look at line 90 and set variable for WiFi Client secure & PubSubCLient 12/23/23
int mqttKeepAlive = 30; // publish temp every x seconds to keep MQTT client connected
// Publishing Topics 
char topic[60];
char topicBase[60];
#define topic_base_path  "msb/traffic/CarCounter"
#define MQTT_PUB_HELLO  "msb/traffic/CarCounter/hello"
#define MQTT_PUB_TEMP  "msb/traffic/CarCounter/temp"
#define MQTT_PUB_TIME  "msb/traffic/CarCounter/time"
#define MQTT_PUB_ENTER_TOTAL  "msb/traffic/CarCounter/EnterTotal"
#define MQTT_PUB_CARS  "msb/traffic/CarCounter/Cars"
#define MQTT_PUB_SUMMARY "msb/traffic/CarCounter/Summary"
#define MQTT_PUB_DAYOFMONTH  "msb/traffic/CarCounter/DayOfMonth"
#define MQTT_PUB_SHOWTOTAL  "msb/traffic/CarCounter/ShowTotal"
#define MQTT_FIRST_BEAM_SENSOR_STATE "msb/traffic/CarCounter/firstBeamSensorState"
#define MQTT_SECOND_BEAM_SENSOR_STATE "msb/traffic/CarCounter/secondBeamSensorState"
#define MQTT_PUB_TTP "msb/traffic/CarCounter/TTP"
#define MQTT_PUB_DAYSRUNNING "msb/traffic/CarCounter/DaysRunning"
#define MQTT_DEBUG_LOG "msb/traffic/CarCounter/debuglog"
#define MQTT_PUB_TIMEBETWEENCARS "msb/traffic/CarCounter/timeBetweenCars"

// Subscribing Topics (to reset values)
//#define MQTT_SUB_TOPIC0  "msb/traffic/CarCounter/EnterTotal"
#define MQTT_SUB_TOPIC1  "msb/traffic/CarCounter/resetDailyCount"
#define MQTT_SUB_TOPIC2  "msb/traffic/CarCounter/resetShowCount"
#define MQTT_SUB_TOPIC3  "msb/traffic/CarCounter/resetDayOfMonth"
#define MQTT_SUB_TOPIC4  "msb/traffic/CarCounter/resetDaysRunning"
#define MQTT_SUB_TOPIC5  "msb/traffic/CarCounter/carCounterTimeout"
#define MQTT_SUB_TOPIC6  "msb/traffic/CarCounter/waitDuration"


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

CarDetectState currentCarDetectState = WAITING_FOR_CAR;
bool carPresentFlag = false; // Flag to ensure a car is only counted once
unsigned long firstBeamTripTime = 0;
unsigned long bothBeamsHighTime = 0;
unsigned long lastCarDetectedMillis = 0;
unsigned long secondBeamTripTime = 0;

const unsigned long personTimeout = 500;        // Time in ms considered too fast for a car, more likely a person
const unsigned long bothBeamsHighThreshold = 750;  // Time in ms for both beams high to consider a car is in the detection zone
unsigned long carCounterTimeout = 60000; // default time for car counter alarm in millis
unsigned long timeToPass = 0;
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

AsyncWebServer server(80);

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
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

/** Display Definitions & variables **/

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const int line1 =0;
const int line2 =9;
const int line3 = 19;
const int line4 = 35;
const int line5 = 50;

//Create Multiple WIFI Object
WiFiMulti wifiMulti;
//WiFiClientSecure espCarCounter;  // Not using htps
WiFiClient espCarCounter;

//const uint32_t connectTimeoutMs = 10000;
uint16_t connectTimeOutPerAP=5000;

/***** MQTT Setup Variables  *****/
PubSubClient mqtt_client(espCarCounter);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
char mqtt_server[] = mqtt_Server;
char mqtt_username[] = mqtt_UserName;
char mqtt_password[] = mqtt_Password;
const int mqtt_port = mqtt_Port;
bool mqtt_connected = false;
bool wifi_connected = false;
int wifi_connect_attempts = 5;

/***** GLOBAL VARIABLES *****/
unsigned int dayOfMonth; // Current Calendar day
unsigned int currentMonth; // Current Month
unsigned int currentHr12; //Current Hour 12 Hour format
unsigned int currentHr24; //Current Hour 24 Hour Format
unsigned int currentMin;
unsigned int currentSec;
unsigned int lastdayOfMonth; // Pervious day's calendar day used to reset running days
unsigned int daysRunning;  // Number of days the show is running.
unsigned int currentTimeMinute; // for converting clock time hh:mm to clock time mm
int totalDailyCars; // total cars counted per day 24/7 Needed for debugging
int totalShowCars; // total cars counted for durning show hours open (5:00 pm to 9:10 pm)
int connectionAttempts = 5; // number of WiFi or MQTT Connection Attempts


unsigned long wifi_connectioncheckMillis = 5000; // check for connection every 5 sec
unsigned long mqtt_connectionCheckMillis = 20000; // check for connection
unsigned long start_MqttMillis; // for Keep Alive Timer
unsigned long start_WiFiMillis; // for keep Alive Timer
unsigned long noCarTimer = 0;
int displayMode = 0;
unsigned long dayMillis = 0;
static char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static unsigned long hourlyCarCount[24] = {0}; // Array for Daily total cars per hour
static float hourlyTemp[24] = {0.0};   // Array to store average temperatures for 24 hours

/** REAL TIME Clock & Time Related Variables **/

RTC_DS3231 rtc;
const char* ampm ="AM";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;
const int   daylightOffset_sec = 3600;
float tempF;
bool flagDaysRunningReset = false;
bool flagMidnightReset = false;
bool flagDailyShowStartReset = false;
bool flagDailySummarySaved = false;
bool flagDailyShowSummarySaved = false;
bool flagHourlyCountsSaved = false;
bool showTime = false;

// sync Time at REBOOT
void SetLocalTime()  {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
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
  tempF = ((rtc.getTemperature()*9/5)+32);
} // end SetLocalTime

// **********FILE NAMES FOR SD CARD *********
File myFile;   //used to write files to SD Card
const String fileName1 = "/EnterTotal.txt"; // DailyTot.txt file to store daily counts in the event of a Failure
const String fileName2 = "/ShowTotal.txt";  // ShowTot.txt file to store season total counts
const String fileName3 = "/DayOfMonth.txt"; // DayOfMonth.txt file to store current day number
const String fileName4 = "/RunDays.txt"; // RunDays.txt file to store days since open
const String fileName5 = "/EnterSummary.csv"; // EnterSummary.csv Stores Daily Totals by Hour and total
const String fileName6 = "/EnterLog.csv"; // EnterLog.csv file to store all car counts for season (was MASTER.CSV)
const String fileName7 = "/ShowSummary.csv"; // Show summary of counts during show (5:00pm to 9:10pm)

// Utility helper to format numbers as strings with thousnds separator
String formatK(int number) {
    String result = "";
    int count = 0;

    // Process the number from the least significant digit to the most
    do {
        int digit = number % 10;
        result = String(digit) + result; // Add the digit to the result
        number /= 10;
        count++;

        // Add a comma after every 3 digits (except at the end)
        if (count % 3 == 0 && number > 0) {
            result = "," + result;
        }
    } while (number > 0);

    return result;
}

void setup_wifi()  {
    Serial.println("Connecting to WiFi");
    display.println("Connecting to WiFi..");
    display.display();
    while(wifiMulti.run(connectTimeOutPerAP) != WL_CONNECTED) {
        Serial.print(".");
    }
    Serial.println("Connected to the WiFi network");
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.display();
  
    display.setCursor(0, line1);
    display.print("SSID: ");
    display.println(WiFi.SSID()); // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP: ");
    Serial.println(ip);
    display.setCursor(0, line2);
    display.print("IP: ");
    display.println(ip);
    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    display.setCursor(0, line3);
    display.print("signal: ");
    display.print(rssi);
    display.println(" dBm");
    display.display();
    // Elegant OTA
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Hi! This is The Car Counter.");
    });
    // You can also enable authentication by uncommenting the below line.
    // ElegantOTA.setAuth("admin", "password");
    ElegantOTA.setID(THIS_MQTT_CLIENT);  // Set Hardware ID
    ElegantOTA.setFWVersion(FWVersion);   // Set Firmware Version
    ElegantOTA.setTitle(OTA_Title);  // Set OTA Webpage Title
    //ElegantOTA.setFilesystemMode(true);  // added 10.16.24.4
    ElegantOTA.begin(&server);    // Start ElegantOTA
    // ElegantOTA callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);
    server.begin();
    Serial.println("HTTP server started");
    delay(1000);
}  // END WiFi Setup

void checkWiFiConnection() {
    static unsigned long lastWiFiCheck = 0;
    static unsigned long lastDisconnectedTime = 0; // Track how long WiFi has been disconnected
    static int reconnectAttempts = 0;

    const unsigned long wifiCheckInterval = 5000;    // Check WiFi every 5 seconds
    const int maxReconnectAttempts = 10;             // Maximum number of reconnection attempts
    const unsigned long maxDisconnectedDuration = 60000; // Max time (1 minute) before system restarts

    // Check WiFi connection at defined intervals
    if (millis() - lastWiFiCheck > wifiCheckInterval) {
        lastWiFiCheck = millis();

        if (wifiMulti.run() != WL_CONNECTED) {
            // Log disconnection and attempt reconnection
            if (reconnectAttempts == 0) {
                Serial.println("WiFi disconnected. Starting reconnection attempts...");
                lastDisconnectedTime = millis(); // Start tracking disconnection duration
            }

            reconnectAttempts++;
            Serial.printf("Reconnection attempt #%d...\n", reconnectAttempts);

            // Attempt to reconnect
            setup_wifi();

            // If maximum attempts are exceeded, log an error
            if (reconnectAttempts >= maxReconnectAttempts) {
                Serial.println("Max WiFi reconnect attempts reached. Will retry after some time...");
                reconnectAttempts = 0; // Reset attempts to allow further retries
            }
        } else {
            // WiFi connected successfully
            if (reconnectAttempts > 0) {
                Serial.println("WiFi reconnected successfully!");
            }
            reconnectAttempts = 0; // Reset attempts counter
            lastDisconnectedTime = 0; // Reset disconnection tracking
        }

        // Check if disconnection has lasted too long
        if (lastDisconnectedTime > 0 && (millis() - lastDisconnectedTime > maxDisconnectedDuration)) {
            Serial.println("WiFi disconnected for too long. Restarting system...");
            ESP.restart(); // Restart the ESP32 to recover
        }
    }
}


/***** MQTT SECTION ******/
// Save messages if MQTT is not connected in Queue
std::queue<String> publishQueue;

// Used to publish MQTT Messages
void publishMQTT(const char *topic, const String &message) {
    if (mqtt_client.connected()) {
        mqtt_client.publish(topic, message.c_str());
    } else {
        Serial.printf("MQTT not connected. Adding to queue: %s -> %s\n", topic, message.c_str());
        publishQueue.push(String(topic) + "|" + message);  // Add message to queue
    }
    start_MqttMillis = millis();
}

// Used to publish Queued Messages
void publishQueuedMessages() {
    while (!publishQueue.empty() && mqtt_client.connected()) {
        String data = publishQueue.front();
        publishQueue.pop();
        
        int delimiterPos = data.indexOf('|');
        if (delimiterPos != -1) {
            String topic = data.substring(0, delimiterPos);
            String message = data.substring(delimiterPos + 1);
            mqtt_client.publish(topic.c_str(), message.c_str());
        }
    }
}

// Used to publish current counts to update Gate Counter every 30 seconds if no car is counted
void KeepMqttAlive()  {
   publishMQTT(MQTT_PUB_TEMP, String(tempF));
   publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
   publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
   start_MqttMillis = millis();
}

//Connects to MQTT Server
void MQTTreconnect() {
    static unsigned long lastReconnectAttempt = 0; // Tracks the last reconnect attempt time
    const unsigned long reconnectInterval = 5000; // Time between reconnect attempts (5 seconds)

    // If the client is already connected, do nothing
    if (mqtt_client.connected()) {
        return;
    }

    // Check if enough time has passed since the last attempt
    if (millis() - lastReconnectAttempt > reconnectInterval) {
        lastReconnectAttempt = millis(); // Update the last attempt time
        Serial.print("Attempting MQTT connection...");

        // Create a unique client ID
        String clientId = THIS_MQTT_CLIENT;

        // Attempt to connect
        if (mqtt_client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("connected.");

            // Subscribe to necessary topics
            mqtt_client.subscribe(MQTT_SUB_TOPIC1); // Reset Daily Count
            mqtt_client.subscribe(MQTT_SUB_TOPIC2); // Reset Show Count
            mqtt_client.subscribe(MQTT_SUB_TOPIC3); // Reset Day of Month
            mqtt_client.subscribe(MQTT_SUB_TOPIC4); // Reset Days Running
            mqtt_client.subscribe(MQTT_SUB_TOPIC5); // Update Car Counter Timeout
            mqtt_client.subscribe(MQTT_SUB_TOPIC6); // Update Sensor Wait Duration

            // Log subscriptions
            Serial.println("Subscribed to MQTT topics.");
            publishMQTT(MQTT_DEBUG_LOG, "MQTT connected and topics subscribed.");
        } else {
            // Log connection failure
            Serial.print("failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(". Will retry...");
        }
    }
}



/***** END MQTT SECTION *****/

// =========== GET SAVED SETUP FROM SD CARD ==========
void getDailyTotal()   {
    // open DAILYTOT.txt to get initial dailyTotal value
    myFile = SD.open(fileName1,FILE_READ);
    if (myFile) {
        while (myFile.available()) {
            totalDailyCars = myFile.parseInt(); // read total
            Serial.print(" Daily cars from file = ");
            Serial.println(totalDailyCars);
        }
        myFile.close();
        publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
    }  
    else  {
        Serial.print("SD Card: Cannot open the file: ");
        Serial.println(fileName1);
    }
} // end getDailyTotal

/** Get season total cars since show opened */
void getShowTotal() {
    myFile = SD.open(fileName2,FILE_READ);
    if (myFile)  {
        while (myFile.available()) {
            totalShowCars = myFile.parseInt(); // read total
            Serial.print(" Total Show cars from file = ");
            Serial.println(totalShowCars);
        }
        myFile.close();
        publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
    }
    else  {
        Serial.print("SD Card: Cannot open the file: ");
        Serial.println(fileName2);
    }
}  // end getShowTotal

/** Get the last calendar day used for reset daily counts) **/
void getDayOfMonth()  {
    myFile = SD.open(fileName3,FILE_READ);
    if (myFile)  {
            while (myFile.available()) {
            lastdayOfMonth = myFile.parseInt(); // read day Number
            Serial.print(" Calendar Day = ");
            Serial.println(lastdayOfMonth);
            publishMQTT(MQTT_PUB_DAYOFMONTH, String(lastdayOfMonth));
            }
        myFile.close();
    }
    else {
        Serial.print("SD Card: Cannot open the file: ");
        Serial.println(fileName3);
        }
}

/** Get number of Days the show has been running) */
void getDaysRunning()  {
    myFile = SD.open(fileName4,FILE_READ);
    if (myFile)  {
        while (myFile.available()) {
            daysRunning = myFile.parseInt(); // read day Number
            Serial.print(" Days Running = ");
            Serial.println(daysRunning);
        }
        myFile.close();
        publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
    }
    else  {
        Serial.print("SD Card: Cannot open the file: ");
        Serial.println(fileName4);
    }
} 

/** Get hourly car counts on reboot */
void getHourlyData() {
    File file = SD.open(fileName5, FILE_READ);
    if (!file) {
        Serial.println("Failed to open file for reading. Initializing hourly data.");
        memset(hourlyCarCount, 0, sizeof(hourlyCarCount)); // Reset to 0
        return;
    }

    String lastLine;
    while (file.available()) {
        lastLine = file.readStringUntil('\n'); // Read until the end of the file
    }
    file.close();

    if (lastLine.length() > 0) {
        char dateBuffer[12];
        int parsedColumns = sscanf(lastLine.c_str(), "%[^,],%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                                   dateBuffer, &hourlyCarCount[0], &hourlyCarCount[1], &hourlyCarCount[2], &hourlyCarCount[3],
                                   &hourlyCarCount[4], &hourlyCarCount[5], &hourlyCarCount[6], &hourlyCarCount[7],
                                   &hourlyCarCount[8], &hourlyCarCount[9], &hourlyCarCount[10], &hourlyCarCount[11],
                                   &hourlyCarCount[12], &hourlyCarCount[13], &hourlyCarCount[14], &hourlyCarCount[15],
                                   &hourlyCarCount[16], &hourlyCarCount[17], &hourlyCarCount[18], &hourlyCarCount[19],
                                   &hourlyCarCount[20], &hourlyCarCount[21], &hourlyCarCount[22], &hourlyCarCount[23]);

        if (parsedColumns == 25) {
            Serial.println("Hourly data successfully loaded from file.");
        } else {
            Serial.println("Error parsing hourly data. Resetting counts.");
            memset(hourlyCarCount, 0, sizeof(hourlyCarCount)); // Reset to 0
        }
    } else {
        Serial.println("File is empty. Initializing hourly data.");
        memset(hourlyCarCount, 0, sizeof(hourlyCarCount)); // Reset to 0
    }
}



/***** UPDATE and SAVE TOTALS TO SD CARD *****/
/** Save the daily Total of cars counted */
void saveDailyTotal() {
    myFile = SD.open(fileName1,FILE_WRITE);
    if (myFile) {  // check for an open failure
        myFile.print(totalDailyCars);
        myFile.close();
        publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
    }
    else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fileName1);
        publishMQTT(MQTT_DEBUG_LOG, "SD Write Failure: Unable to save daily total.");
    }
    publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
}

/* Save the grand total cars file for season  */
void saveShowTotal()  {  
    myFile = SD.open(fileName2,FILE_WRITE);
    if (myFile) {
        myFile.print(totalShowCars); // only count cars between 4:55 pm and 9:10 pm
        myFile.close();
        Serial.print(F("Updating Show Total "));
        Serial.print(totalShowCars);
        Serial.print(F(" Car # "));
    }
    else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fileName2);
    }
    publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));  
}

// Save the calendar day to file ----- */
void saveDayOfMonth()  {
    myFile = SD.open(fileName3,FILE_WRITE);
    if (myFile) {
        myFile.print(dayOfMonth);
        myFile.close();
    }
    else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fileName3);
    }
    publishMQTT(MQTT_PUB_DAYOFMONTH, String(dayOfMonth));
    }

/** Save number of days the show has been running */
void saveDaysRunning() {
    myFile = SD.open(fileName4,FILE_WRITE);
    if (myFile) {
            myFile.print(daysRunning);
            myFile.close();
    }
    else  {
            Serial.print(F("SD Card: Cannot open the file: "));
            Serial.println(fileName4);
    }
    publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
}


// Save cars counted each hour in the event of a reboot
void saveHourlyCounts() {
    DateTime now = rtc.now();
    static int lastHour = -1; // Track the last hour the function was executed

    int currentHour = now.hour();

    // Ensure this function is only called once per hour
    if (currentHour == lastHour) {
        return;
    }
    lastHour = currentHour; // Update last executed hour

    // Publish traffic counts for the current hour
    for (int i = 0; i < 24; i++) {
        char hourlyTopic[100];
        snprintf(hourlyTopic, sizeof(hourlyTopic), "%s%02d", MQTT_PUB_CARS, i); // Topic: msb/traffic/CarCounter/Cars/hh
        publishMQTT(hourlyTopic, String(hourlyCarCount[i])); // Publish the count for the hour

        // Debug log
        //Serial.printf("Published hourly count for %02d: %d cars\n", i, hourlyCarCount[i]);
    }

    // Publish daily total to a separate topic
    publishMQTT("msb/traffic/CarCounter/Cars", String(totalDailyCars));
    //Serial.printf("Published daily total: %d cars\n", totalDailyCars);

    // Save data to the SD card or perform any other end-of-hour actions as necessary
    char dateBuffer[12];
    snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d", now.year(), now.month(), now.day());

    File hourlyFile = SD.open(fileName5, FILE_APPEND);
    if (!hourlyFile) {
        Serial.println("Failed to open file for writing hourly data.");
        return;
    }

    // Write the hourly data to the SD card
    hourlyFile.printf("%s,%02d,%d\n", dateBuffer, currentHour, hourlyCarCount[currentHour]);
    hourlyFile.close();
    Serial.printf("Saved hourly data for %02d: %d cars\n", currentHour, hourlyCarCount[currentHour]);
}

// Save and Publish Show Totals
void saveDailyShowSummary() {
    DateTime now = rtc.now();

    // Define show hours (5 PM to 9 PM)
    const int showStartHour = 17; // 5 PM
    const int showEndHour = 20;  // Up to 9 PM

    // Calculate cumulative totals for each key hour
    int cumulative6PM = hourlyCarCount[17]; // Total at 6 PM
    int cumulative7PM = cumulative6PM + hourlyCarCount[18]; // Total at 7 PM
    int cumulative8PM = cumulative7PM + hourlyCarCount[19]; // Total at 8 PM
    int cumulative9PM = cumulative8PM + hourlyCarCount[20]; // Total at 9 PM

    // Include additional cars detected between 9:00 PM and 9:10 PM
    if (now.hour() == 21 && now.minute() <= 10) {
        cumulative9PM += hourlyCarCount[21];
    }

    // Calculate the average temperature during show hours (5 PM to 9 PM)
    float showTempSum = 0.0;
    int showTempCount = 0;

    for (int i = showStartHour; i <= showEndHour; i++) {
        if (hourlyTemp[i] != 0.0) { // Include all valid temperatures
            showTempSum += hourlyTemp[i];
            showTempCount++;
        }
    }

    float showAverageTemp = (showTempCount > 0) ? (showTempSum / showTempCount) : 0.0;

    // Open file for appending
    File summaryFile = SD.open(fileName7, FILE_APPEND);
    if (!summaryFile) {
        Serial.println("Failed to open daily show summary file.");
        publishMQTT(MQTT_DEBUG_LOG, "Failed to open daily show summary file.");
        return;
    }

    // Format the current date
    char dateBuffer[12];
    snprintf(dateBuffer, sizeof(dateBuffer), "%04d-%02d-%02d", now.year(), now.month(), now.day());

    // format with thousands separator
    String totalShowCarsK = formatK(totalShowCars);
    String cumulative9PMK = formatK(cumulative9PM);

    // Append the show summary data
    summaryFile.printf("%s,%d,%d,%d,%d,%d,%.1f\n",
                       dateBuffer,       // Current date
                       cumulative6PM,    // Cumulative total up to 6 PM
                       cumulative7PM,    // Cumulative total up to 7 PM
                       cumulative8PM,    // Cumulative total up to 8 PM
                       cumulative9PM,    // Cumulative total up to 9 PM, including 9:10 PM cars
                       cumulative9PM,    // Total show cars
                       showAverageTemp,  // Average temperature during show hours
                       daysRunning);     // Total days running
    summaryFile.close();

    // Publish show summary data to MQTT
    publishMQTT(MQTT_PUB_SUMMARY, String("Date: ") + dateBuffer +
                                       ", 6PM: " + cumulative6PM +
                                       ", 7PM: " + cumulative7PM +
                                       ", 8PM: " + cumulative8PM +
                                       ", 9PM: " + cumulative9PMK +
                                       ", ShowTotal: " + totalShowCarsK +
                                       ", ShowAvgTemp: " + String(showAverageTemp, 1) +
                                       ", DaysRunning: " + String(daysRunning));
    
    Serial.printf("Daily show summary written: %s, 6PM: %d, 7PM: %d, 8PM: %d, 9PM: %d, ShowTotal: %d, Avg Temp: %.1f°F.\n",
                  dateBuffer, cumulative6PM, cumulative7PM, cumulative8PM, cumulative9PM, totalShowCars , showAverageTemp);
}


void getSavedValuesOnReboot() {
    DateTime now = rtc.now();

    // Read the last recorded day from the SD card
    getDayOfMonth();

    // Check if the ESP32 is rebooting on a new day
    if (now.day() != lastdayOfMonth) {
        dayOfMonth = now.day(); // Update to the current day
        saveDayOfMonth(); // Save the new day to the SD card
        totalDailyCars = 0; // Reset daily car count
        saveDailyTotal(); // Save the reset value to the SD card

        // Increment days running, except on Christmas Eve
        if (!(now.month() == 12 && now.day() == 24) ) {
            daysRunning++;
            saveDaysRunning(); // Save updated days running to the SD card
            publishMQTT(MQTT_DEBUG_LOG, "Rebooted, Day of Month Changed, Days Running Increased.");
        }

        // Log the update
        Serial.println("ESP32 reboot detected on a new day. Counts reset/updated.");
        publishMQTT(MQTT_DEBUG_LOG, "Rebooted, Counts reset/updated for new day.");
    } else {
        // If the day has not changed, reload the existing totals
        getDailyTotal();   // Reload daily car count
        getShowTotal();    // Reload show total
        getDaysRunning();  // Reload days running
        getHourlyData();   // Reload Hourly Count Data

        // Log the reload
        Serial.println("ESP32 reboot detected on the same day. Reloading saved counts.");
        publishMQTT(MQTT_DEBUG_LOG, "Rebooted, Counts reloaded for same day.");
    }
}
/***** END OF FILE UPDATES *****/

/*** MQTT CALLBACK TOPICS for Reveived messages ****/
void callback(char* topic, byte* payload, unsigned int length)  {
    /* 
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
      Serial.print((char)payload[i]);
    }
    payload[length] = '\0';
    Serial.println();
    */
    char message[length + 1];
    strncpy(message, (char*)payload, length);
    message[length] = '\0'; // Safely null-terminate the payload

    if (strcmp(topic, MQTT_SUB_TOPIC1) == 0)  {
        /* Topic used to manually reset Enter Daily Cars */
        totalDailyCars = atoi((char *)payload);
        saveDailyTotal();
        Serial.println(F(" Car Counter Updated"));
        publishMQTT(MQTT_PUB_HELLO, "Daily Total Updated");
    } else if (strcmp(topic, MQTT_SUB_TOPIC2) == 0)  {
        /* Topic used to manually reset Total Show Cars */
        totalShowCars = atoi((char *)payload);
        saveShowTotal();
        Serial.println(F(" Show Counter Updated"));
        publishMQTT(MQTT_PUB_HELLO, "Show Counter Updated");
    } else if (strcmp(topic, MQTT_SUB_TOPIC3) == 0)  {
        /* Topic used to manually reset Calendar Day */
        dayOfMonth = atoi((char *)payload);
        saveDayOfMonth();
        Serial.println(F(" Calendar Day of Month Updated"));
        publishMQTT(MQTT_PUB_HELLO, "Calendar Day Updated");
    } else if (strcmp(topic, MQTT_SUB_TOPIC4) == 0)  {
        /* Topic used to manually reset Days Running */
        daysRunning = atoi((char *)payload);
        saveDaysRunning();
        Serial.println(F(" Days Running Updated"));
        publishMQTT(MQTT_PUB_HELLO, "Days Running Updated");
    } else if (strcmp(topic, MQTT_SUB_TOPIC5) == 0)  {
        // Topic used to change car counter timeout
        carCounterTimeout = atoi((char *)payload);
        Serial.println(F(" Counter Alarm Timer Updated"));
        publishMQTT(MQTT_PUB_HELLO, "Car Counter Timeout Updated");
    } else if (strcmp(topic, MQTT_SUB_TOPIC6) == 0)  {
        // Topic used to change car counter timeout
        waitDuration = atoi((char *)payload);
        Serial.println(F(" Beam Sensor Durtion Updated"));
        publishMQTT(MQTT_PUB_HELLO, "Beam Sensor Duration Updated"); 
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
    static int tempReadingsCount = 0;      // Number of temperature readings
    static float tempReadingsSum = 0.0;    // Sum of temperature readings
    static float hourlyTemp[24] = {0.0};   // Array to store average temperatures for 24 hours

    DateTime now = rtc.now();
    int nowHour = now.hour();

    // Check if the hour has changed
    if (nowHour != lastPublishedHour) {
        // Calculate and store the average for the previous hour
        if (tempReadingsCount > 0 && lastPublishedHour >= 0) {
            hourlyTemp[lastPublishedHour] = tempReadingsSum / tempReadingsCount;

            // Publish the average temperature for the completed hour
            char topic[50];
            snprintf(topic, sizeof(topic), "%s/hourly/%02d", MQTT_PUB_TEMP, lastPublishedHour);
            publishMQTT(topic, String(hourlyTemp[lastPublishedHour], 1)); // Publish with 1 decimal place

            // Print to serial monitor
            Serial.printf("Hour %02d average temperature published: %.1f°F\n", lastPublishedHour, hourlyTemp[lastPublishedHour]);
        }

        // Reset for the new hour
        lastPublishedHour = nowHour;
        tempReadingsSum = 0.0;
        tempReadingsCount = 0;
    }

    // Add the current temperature reading to the sum
    float tempF = ((rtc.getTemperature() * 9 / 5) + 32);
    tempReadingsSum += tempF;
    tempReadingsCount++;
}

// Car Counted, increment the counter by 1 and append to the Enterlog.csv log file on the SD card
void beamCarDetect() {
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
    saveDailyTotal();  // Save Daily Total on SD Card
    // Increment hourly car count
    int currentHour = now.hour();
    hourlyCarCount[currentHour]++;

    // Construct the MQTT topic dynamically
    char formattedTopic[100]; // Buffer for the full topic
    snprintf(formattedTopic, sizeof(formattedTopic), "%s%02d", MQTT_PUB_CARS, currentHour); // Append the two-digit hour

    // Publish the updated hourly count
    publishMQTT(formattedTopic, String(hourlyCarCount[currentHour]));

    // increase Show Count only when show is open
    if (showTime == true)  {
        totalShowCars ++;   
        saveShowTotal();  // update show total count in event of power failure during show hours
    }
    Serial.print(totalDailyCars) ;  
    // open file for writing Car Data
    myFile = SD.open(fileName6, FILE_APPEND); //Enterlog.csv
    if (myFile) {
        myFile.print(now.toString(buf2));
        myFile.print(", ");
        myFile.print (timeToPass) ; 
        myFile.print(", 1 , "); 
        myFile.print (totalDailyCars) ; 
        myFile.print(", ");
        myFile.println(tempF);
        myFile.close();
        Serial.print(F(" Car "));
        Serial.print(totalDailyCars);
        Serial.println(F(" Logged to SD Card."));
        publishMQTT(MQTT_PUB_HELLO, "Car Counter Working");
        publishMQTT(MQTT_PUB_TEMP, String(tempF));
        publishMQTT(MQTT_PUB_TIME, now.toString(buf2));
        publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
        start_MqttMillis = millis();
    } 
    else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fileName6);
    }
} /* END Beam Car Detect*/

/** State Machine to Detect and Count Cars */
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
                publishMQTT(MQTT_DEBUG_LOG, "First Beam High");
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
                    publishMQTT(MQTT_DEBUG_LOG, "State Changed Both Beams High");
                    digitalWrite(greenArchPin, LOW);  // Turn off green arch
                    digitalWrite(redArchPin, HIGH);   // Turn on red arch
                 }
                
            } else if (firstBeamState == 0) {
                // If the first beam is cleared before the second beam is triggered, reset
                currentCarDetectState = WAITING_FOR_CAR;
                publishMQTT(MQTT_DEBUG_LOG, "1st Beam Low, Changed Waiting for Car");
                digitalWrite(greenArchPin, HIGH); // Turn on green arch
            }
            break;

        case BOTH_BEAMS_HIGH:
            if (currentMillis - firstBeamTripTime == carCounterTimeout) {
                // Set Alarm if car is stuck at car counter
                publishMQTT(MQTT_PUB_HELLO, "Check Car Counter!");
            }         
            if (secondBeamState == 0 && carPresentFlag) {
                currentCarDetectState = CAR_DETECTED;
                publishMQTT(MQTT_DEBUG_LOG, "Changed state to Car Detected");
            }
            break;

        case CAR_DETECTED:
            if (carPresentFlag) {
                beamCarDetect(); // Count the car and update files
                unsigned long timeToPass = currentMillis - firstBeamTripTime;
                publishMQTT(MQTT_PUB_TTP, String(timeToPass));
                publishMQTT(MQTT_DEBUG_LOG, "Car Counted Successfully!!");
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
                publishMQTT(MQTT_DEBUG_LOG, "Idle-Waiting For Car");
            }
            break;
    }
}
// END CAR DETECTION

// CHECK AND CREATE FILES on SD Card and WRITE HEADERS if Needed
void checkAndCreateFile(const String &fileName, const String &header = "") {
    if (!SD.exists(fileName)) {
        Serial.printf("%s doesn't exist. Creating file...\n", fileName.c_str());
        myFile = SD.open(fileName, FILE_WRITE);
        myFile.close();
        if (!header.isEmpty()) {
            myFile = SD.open(fileName, FILE_APPEND);
            myFile.println(header);
            myFile.close();
            Serial.printf("Header written to %s\n", fileName.c_str());
        }
    } else {
        Serial.printf("%s exists on SD Card.\n", fileName.c_str());
    }
}

/** Initilaize microSD card */
void initSDCard()  {
    if(!SD.begin(PIN_SPI_CS))  {
        Serial.println("Card Mount Failed");
        //Serial.println(F("SD CARD FAILED, OR NOT PRESENT!"));
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0,line1);
        display.println("Check SD Card");
        display.display();
        while (1); // stop the program and check SD Card
        return;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE)  {
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);

    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    Serial.println(F("SD CARD INITIALIZED."));
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,line1);
    //display.println("SD Card Ready");
    display.printf("SD Card Size: %lluMB\n", cardSize);
    display.display();
    // NECESSARY FILES NEEDED TO RUN
    checkAndCreateFile(fileName1);
    checkAndCreateFile(fileName2);
    checkAndCreateFile(fileName3);
    checkAndCreateFile(fileName4);
    checkAndCreateFile(fileName5, "Date,Hr 00,Hr 01,Hr 02,Hr 03,Hr 04,Hr 05,Hr 06,Hr 07,Hr 08,Hr 09,Hr 10,Hr 11,Hr 12,Hr 13,Hr 14,Hr 15,Hr 16,Hr 17,Hr 18,Hr 19,Hr 20,Hr 21,Hr 22,Hr 23");
    checkAndCreateFile(fileName6, "Date Time,TimeToPass,Car#,TotalDailyCars,Temp");
    checkAndCreateFile(fileName7, "Date,DailyCars,6PM,7PM,8PM,9PM,DaysRunning,ShowTotal,DailyAvgTemp,DaysRunning");
}

/** Resets the hourly count array at midnight */
void resetHourlyCounts() {
    for (int i = 0; i < 24; i++) {
        hourlyCarCount[i] = 0; // Reset the hourly counts
    }
    // Log the reset
    Serial.println("Hourly counts have been reset.");
    publishMQTT(MQTT_DEBUG_LOG, "Hourly counts reset for the new day.");
}

/* Resets counts at Start of Show and Midnight */
void timeTriggeredEvents() {
    DateTime now = rtc.now();

    // Reset hourly counts at midnight
    if (now.hour() == 23 && now.minute() == 59 && !flagMidnightReset) { 
        resetHourlyCounts();  // reset array for collecting hourly car counts      
        totalDailyCars = 0;   // reset total daily cars to 0 at midnight
        saveDailyTotal();
        publishMQTT(MQTT_DEBUG_LOG, "Total cars reset at Midnight");
        flagMidnightReset = true;
    }
    
    // Increment days running only if not Christmas Eve
    if (now.day() != lastdayOfMonth) {
        if (!(now.month() == 12 && now.day() == 24) && !flagDaysRunningReset) {
            daysRunning++;
            saveDaysRunning();
            publishMQTT(MQTT_DEBUG_LOG, "Days running: " + String(daysRunning));
        }
        dayOfMonth = now.day(); // Update day of month
        saveDayOfMonth(); // Save new day of month
        getDayOfMonth(); // Get Day of month
        publishMQTT(MQTT_DEBUG_LOG, "Day of month: " + String(dayOfMonth));
        flagDaysRunningReset = true;
    }

    // Reset total daily cars for show to 0 at 4:59 PM
    if (now.hour() == 16 && now.minute() == 59 && !flagDailyShowStartReset) {
        totalDailyCars = 0;
        saveDailyTotal();
        publishMQTT(MQTT_DEBUG_LOG, "Total cars reset at 4:59 PM");
        flagDailyShowStartReset = true;
    }

    // Save daily summary at 9:10 PM
    if (now.hour() == 21 && now.minute() == 10 && !flagDailyShowSummarySaved) {
        saveDailyShowSummary();
         publishMQTT(MQTT_DEBUG_LOG, "Daily Show Summary Saved");
        flagDailyShowSummarySaved = true;
    }


    // Check for hourly data save
    if (now.minute() == 59 && now.second() == 59 && !flagHourlyCountsSaved) {
        saveHourlyCounts();
        flagHourlyCountsSaved = true;
    }

    // Reset flags for next day
    if (now.hour() == 0 && now.minute() == 1 ) { 
        flagDaysRunningReset = false;
        flagMidnightReset = false;
        flagDailyShowStartReset = false;
        flagDailySummarySaved = false;
        flagDailyShowSummarySaved = false;
        flagHourlyCountsSaved = false;
        publishMQTT(MQTT_DEBUG_LOG, "Run once flags reset for new day");
    }
}



// Update OLED Display while running
void updateDisplay() {
    DateTime now = rtc.now();
    float tempF = ((rtc.getTemperature() * 9 / 5) + 32); // Get temperature in Fahrenheit
    int currentHr24 = now.hour();
    int currentHr12 = currentHr24 > 12 ? currentHr24 - 12 : (currentHr24 == 0 ? 12 : currentHr24);
    const char* ampm = currentHr24 < 12 ? "AM" : "PM";

    // Example: Display totalShowCars with thousands separator
    String totalShowCarsK = formatK(totalShowCars);

    // Clear display and set formatting
    display.clearDisplay();
    display.setTextColor(WHITE);
    //display.setFont(&FreeSans12pt7b);

    // Line 1: Date and Day of the Week
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("%s %s %02d, %04d", days[now.dayOfTheWeek()], months[now.month() - 1], now.day(), now.year());

    // Line 2: Time and Temperature
    display.setCursor(0, 10);
    display.printf("%02d:%02d:%02d %s  %d F", currentHr12, now.minute(), now.second(), ampm, (int)tempF);

    // Line 3: Days Running and Show Total
    display.setCursor(0, 20);
    display.printf("Day %d  Total: %d", daysRunning, totalShowCarsK);

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
    mqtt_client.setServer(mqtt_server, mqtt_port);
    mqtt_client.setCallback(callback);

    // MQTT Reconnection with login credentials
    MQTTreconnect(); // Ensure MQTT is connected

    //If RTC not present, stop and check battery
    if (! rtc.begin()) {
        Serial.println("Could not find RTC! Check circuit.");
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0,line1);
        display.println("Clock DEAD");
        display.display();
        while (1);
    }

    // Get NTP time from Time Server 
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    SetLocalTime();
    
    /* Set Input Pins Beam Pins set to pulldown 10/22/24 GAL undefined before
    Beam Pins LOW when no car is present and go HIGH when beam is broken */
    pinMode(firstBeamPin, INPUT_PULLDOWN);
    pinMode(secondBeamPin, INPUT_PULLDOWN);
    pinMode(redArchPin, OUTPUT);
    pinMode(greenArchPin, OUTPUT);
    digitalWrite(redArchPin, HIGH);
    digitalWrite(greenArchPin, HIGH);
    
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0, line3);
    display.println(" Starting");
    display.println("CarCounter");

    Serial.println  ("Initializing Car Counter");
    Serial.print("Temperature: ");
    tempF=((rtc.getTemperature()*9/5)+32);
    Serial.print(tempF);
    Serial.println(" F");
    display.display();
    delay(500); // display Startup
  
    //Initialize SD Card
    initSDCard();  // Initialize SD card and ready for Read/Write

    //on reboot, get totals saved on SD Card
    //getDailyTotal();  /*Daily total that is updated with every detection*/
    //getShowTotal();   /*Saves Show Total*/
    //getDayOfMonth();  /* Get Last Calendar Day*/ 
    //getDaysRunning(); /*Needs to be reset 1st day of show*/
    getSavedValuesOnReboot();  // Update/reset counts based on reboot day
    


    // Setup MDNS
    if (!MDNS.begin(THIS_MQTT_CLIENT)) {
        Serial.println("Error starting mDNS");
        return;
    }

    delay(1000);
    start_MqttMillis = millis();
} /***** END SETUP ******/

void loop() {
    static int lastHour = -1; // Tracks the last hour the function was executed
    static int lastMinute = -1; // Tracks the last minute the function was executed

    DateTime now = rtc.now();
    int currentHour = now.hour();
    int currentMinute = now.minute();

    // Run the function only when the hour changes and once in the first minute
    if (currentHour != lastHour || (currentHour == lastHour && currentMinute == 0 && lastMinute != 0)) {
        saveHourlyCounts(); // Save data for the new hour
        lastHour = currentHour; // Update the last executed hour
        lastMinute = currentMinute; // Update the last executed minute
        Serial.printf("Hourly data saved for hour %02d.\n", currentHour);

        // Publish debug log
        char debugMessage[100];
        snprintf(debugMessage, sizeof(debugMessage), "Hourly data saved for hour %02d.", currentHour);
        publishMQTT(MQTT_DEBUG_LOG, debugMessage);
    }


    ElegantOTA.loop();  // Keep OTA Updates Alive

      // Only record show totals when show is open
    currentTimeMinute = now.hour()*60 + now.minute();
    if (currentTimeMinute >= showStartTime && currentTimeMinute <= showEndTime) {
        // show is running and save counts
        showTime = true;
    } 
    else {
        showTime = false;
    }

    // Update the display
    updateDisplay();

    // Check and maintain WiFi connection
    checkWiFiConnection();

    /*****IMPORTANT***** Reset Car Counter at 4:59:59 pm ****/
    //Write Totals at 9:10:00 pm. Gate should close at 9 PM. Allow for any cars in line to come in
    /* Reset Counts at Midnight when controller ESP32 running 24/7 */
    timeTriggeredEvents();

    // Detect cars
    detectCar();   

    // Play pattern when idle
    if (millis() - noCarTimer >= 30000 && secondBeamState == 0) {
        playPattern();
    }

    // Update temperature readings for hourly average
    averageHourlyTemp();

    // Keep MQTT connection alive
    if (mqtt_client.connected()) {
        publishQueuedMessages();
        mqtt_client.loop();  //  Kee[ MQTT Client Alive
    } else {
        MQTTreconnect();  // Ensure MQTT connection is maintained
    }
    
    // Publish Car counts to keep Gate Counter Updated
    if ((millis() - start_MqttMillis) > (mqttKeepAlive * 1000)) {
        KeepMqttAlive();
    }

} /***** Repeat Loop *****/