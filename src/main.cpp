/*
Car counter Interface to MQTT by Greg Liebig gliebig@sheboyganlights.org
Initial Build 12/5/2023 12:15 pm
Changed time format YYYY-MM-DD hh:mm:ss 12/13/23

Changelog
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
#define FWVersion "24.12.04.7" // Firmware Version
#define OTA_Title "Car Counter" // OTA Title
#define DS3231_I2C_ADDRESS 0x68 // Real Time Clock
#define firstBeamPin 33
#define secondBeamPin 32
#define redArchPin 25
#define greenArchPin 26
#define PIN_SPI_CS 5 // The ESP32 pin GPIO5
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// #define MQTT_KEEPALIVE 30 //removed 10/16/24

unsigned int waitDuration = 950; // minimum millis for secondBeam to be broken needed to detect a car
unsigned int showStartTime = 16*60 + 55; // Show (counting) starts at 4:55 pm
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
#define MQTT_PUB_CARS_17  "msb/traffic/CarCounter/Cars_17"
#define MQTT_PUB_CARS_18  "msb/traffic/CarCounter/Cars_18"
#define MQTT_PUB_CARS_19  "msb/traffic/CarCounter/Cars_19"
#define MQTT_PUB_CARS_20  "msb/traffic/CarCounter/Cars_20"
#define MQTT_PUB_CARS_21  "msb/traffic/CarCounter/Cars_21"
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
int carsHr17; // total cars before show starts
int carsHr18; // total cars 1st hour ending 18:00 (5:00 pm to 6:00 pm)
int carsHr19; // total cars 2nd hour ending 19:00 (6:00 pm to 7:00 pm)
int carsHr20; // total cars 3rd hour ending 20:00 (7:00 pm to 8:00 pm)
int carsHr21; // total cars 4th hour ending 21:00 (8:00 pm to 9:10 pm)

unsigned long wifi_connectioncheckMillis = 5000; // check for connection every 5 sec
unsigned long mqtt_connectionCheckMillis = 20000; // check for connection
unsigned long start_MqttMillis; // for Keep Alive Timer
unsigned long start_WiFiMillis; // for keep Alive Timer
unsigned long noCarTimer = 0;
int displayMode = 0;
unsigned long dayMillis = 0;
char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
unsigned long hourlyCarCount[24] = {0}; // Array for Daily total cars per hour

/** REAL TIME Clock & Time Related Variables **/

RTC_DS3231 rtc;
const char* ampm ="AM";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;
const int   daylightOffset_sec = 3600;
float tempF;
bool hasRun = false;
bool showTime = false;

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
//const String fileName7 = "/ShowSummary.csv"; // Shows summary of counts during show (4:55pm to 9:10pm)





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

std::queue<String> publishQueue;

void publishMQTT(const char *topic, const String &message) {
    if (mqtt_client.connected()) {
        mqtt_client.publish(topic, message.c_str());
    } else {
        Serial.printf("MQTT not connected. Adding to queue: %s -> %s\n", topic, message.c_str());
        publishQueue.push(String(topic) + "|" + message);  // Add message to queue
    }
    start_MqttMillis = millis();
}

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


void KeepMqttAlive()  {
   publishMQTT(MQTT_PUB_TEMP, String(tempF));
   publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
   publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
   start_MqttMillis = millis();
}

void MQTTreconnect()  {
  // Loop until we’re reconnected
  while (!mqtt_client.connected())  {
    Serial.print("Attempting MQTT connection… ");
    String clientId = THIS_MQTT_CLIENT;
    // Attempt to connect
    if (mqtt_client.connect(clientId.c_str(), mqtt_username, mqtt_password))
    {
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0,0);
      display.println("MQTT Connect");
      display.display();
      Serial.println("connected!");
      Serial.println("Waiting for Car...");
      // Once connected, publish an announcement…
      publishMQTT(MQTT_PUB_HELLO, "Car Counter ONLINE!");
      publishMQTT(MQTT_PUB_TEMP, String(tempF));
      publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
      // … and resubscribe
      mqtt_client.subscribe(MQTT_PUB_HELLO);
    } 
    else
    {
      Serial.print("failed, rc = ");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0,line5);
      display.println("MQTT Error");
      display.display();
    }
  }  //END while
 // mqtt_client.subscribe(MQTT_SUB_TOPIC0);
  mqtt_client.subscribe(MQTT_SUB_TOPIC1);
  mqtt_client.subscribe(MQTT_SUB_TOPIC2);
  mqtt_client.subscribe(MQTT_SUB_TOPIC3);
  mqtt_client.subscribe(MQTT_SUB_TOPIC4);
  mqtt_client.subscribe(MQTT_SUB_TOPIC5);
} // END MQTT Reconnect



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

/***** UPDATE TOTALS TO SD CARD *****/
void updateHourlyTotals()  {
  char hourPad[6];
  sprintf(hourPad,"%02d", currentHr24);
  hourlyCarCount[currentHr24]= totalDailyCars; //write daily total cars to array each hour
  strcpy (topicBase, topic_base_path);
  strcat (topicBase, "/daily/hour/");
  //strcat (topicBase, String(currentHr24).c_str());
  strcat (topicBase, hourPad);
      publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
}

/** Save the daily Total of cars counted */
void updateDailyTotal() {
  myFile = SD.open(fileName1,FILE_WRITE);
  if (myFile) {  // check for an open failure
     myFile.print(totalDailyCars);
     myFile.close();
     publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
  }
  else {
    Serial.print(F("SD Card: Cannot open the file: "));
    Serial.println(fileName1);
  }
  publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
}

/* Save the grand total cars file for season  */
void updateShowTotal()  {  
   myFile = SD.open(fileName2,FILE_WRITE);
   if (myFile) 
   {
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

// Save the calendar day 1 seond past midnight to file ----- */
void updateDayOfMonth()  {
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
void updateDaysRunning() {
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

/** Resets the hourly count array at midnight */
void resetHourlyCounts() {
    for (int i = 0; i < 24; i++) {
        hourlyCarCount[i] = 0;
    }
    publishMQTT(MQTT_DEBUG_LOG, "Hourly car counts reset at midnight");
}


/** Saves the daily show numbers */
void WriteDailySummary() {
  // Write totals daily at end of show (EOS Totals)
  DateTime now = rtc.now();
  tempF = ((rtc.getTemperature()*9/5)+32);
  char buf2[] = "YYYY-MM-DD hh:mm:ss";
  Serial.print(now.toString(buf2));
  Serial.print(", Temp = ");
  Serial.print(tempF);
  Serial.print(", ");
  Serial.print(totalDailyCars) ;  
  // open file for writing Car Data
  myFile = SD.open(fileName5, FILE_APPEND);
  if (myFile)  {
      myFile.print(now.toString(buf2));
      myFile.print(", "); 
      for (int i = 17; i<=21; i++) {
          myFile.print(hourlyCarCount[i]);
          myFile.print(", ");
      }    
      myFile.println(tempF);
      myFile.close();
      Serial.println(F(" = Daily Summary Recorded SD Card."));
  }
  else  {
      Serial.print(F("SD Card: Cannot open the file: "));
      Serial.println(fileName5);
  }
    // Publish Totals
  publishMQTT(MQTT_PUB_TEMP, String(tempF));
  publishMQTT(MQTT_PUB_TIME, now.toString(buf2));
  publishMQTT(MQTT_PUB_CARS_18, String(hourlyCarCount[18]));
  publishMQTT(MQTT_PUB_CARS_19, String(hourlyCarCount[19]));
  publishMQTT(MQTT_PUB_CARS_20, String(hourlyCarCount[20]));
  publishMQTT(MQTT_PUB_CARS_21, String(hourlyCarCount[21]));
  publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
  publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
  publishMQTT(MQTT_PUB_HELLO, "Enter Summary File Updated");
  hasRun = true;
}

/* Resets counts at Midnight */
void checkMidnightReset() {
    unsigned long currentMillis = millis();
    static unsigned long lastResetMillis = 0;
    int currentHour = (currentMillis / (1000 * 60 * 60)) % 24;
    if (currentHour == 0 && (currentMillis - lastResetMillis > 3600000)) { // Check if it is midnight and avoid multiple resets
        resetHourlyCounts();
        daysRunning++;
        updateDaysRunning();
        dayOfMonth = (dayOfMonth % 31) + 1; // Assuming month has 31 days
        updateDayOfMonth();
        publishMQTT(MQTT_DEBUG_LOG, "Days running: " + String(daysRunning));
        publishMQTT(MQTT_DEBUG_LOG, "Day of month: " + String(dayOfMonth));
        lastResetMillis = currentMillis;
    }
    else if (currentHour == 21 && (currentMillis - lastResetMillis > 3540000)) { // Check if it is 9:10 pm
        WriteDailySummary();
        lastResetMillis = currentMillis;
    }
} /***** END OF FILE UPDATES *****/

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

/* Car Counted, increment the counter by 1 and append to the Enterlog.csv log file on the SD card */
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
  updateDailyTotal();  // Update Daily Total on SD Card to retain numbers with reboot
  // Increment hourly car count
  int currentHour = (millis() / (1000 * 60 * 60)) % 24;
  hourlyCarCount[currentHour]++;
  String topic = "MQTT_PUB_CARS_" + String(currentHour);
  publishMQTT(topic.c_str(), String(hourlyCarCount[currentHour]));
  // increase Show Count only when show is open
  if (showTime == true)  {
     totalShowCars ++;   
     updateShowTotal();  // update show total count in event of power failure during show hours
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

/** Used to create files and headers on SD Card */
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
   // Check and create files
  checkAndCreateFile(fileName1);
  checkAndCreateFile(fileName2);
  checkAndCreateFile(fileName3);
  checkAndCreateFile(fileName4);
  checkAndCreateFile(fileName5, "Date,Hour-17,Hour-18,Hour-19,Hour-20,Hour-21,Total,Temp");
  checkAndCreateFile(fileName6, "Date Time,TimeToPass,Car#,TotalDailyCars,Temp");
}

/*** MQTT CALLBACK TOPICS ****/
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
      updateDailyTotal();
      Serial.println(F(" Car Counter Updated"));
      publishMQTT(MQTT_PUB_HELLO, "Daily Total Updated");
  } else if (strcmp(topic, MQTT_SUB_TOPIC2) == 0)  {
      /* Topic used to manually reset Total Show Cars */
      totalShowCars = atoi((char *)payload);
      updateShowTotal();
      Serial.println(F(" Show Counter Updated"));
      publishMQTT(MQTT_PUB_HELLO, "Show Counter Updated");
  } else if (strcmp(topic, MQTT_SUB_TOPIC3) == 0)  {
      /* Topic used to manually reset Calendar Day */
      dayOfMonth = atoi((char *)payload);
      updateDayOfMonth();
      Serial.println(F(" Calendar Day of Month Updated"));
      publishMQTT(MQTT_PUB_HELLO, "Calendar Day Updated");
  } else if (strcmp(topic, MQTT_SUB_TOPIC4) == 0)  {
     /* Topic used to manually reset Days Running */
      daysRunning = atoi((char *)payload);
      updateDaysRunning();
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
      Serial.println(F(" Sensor Durtion Updated"));
      publishMQTT(MQTT_PUB_HELLO, "Sensor Duration Updated"); 
 } 
 }/***** END OF CALLBACK TOPICS *****/

/******  BEGIN SETUP ******/
void setup() {
  Serial.begin(115200);
  ElegantOTA.setAutoReboot(true);
  ElegantOTA.setFilesystemMode(true);

  //Initialize Display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, line1);
  display.display();
  


    // List of approved WiFi AP's
  WiFi.mode(WIFI_STA);  
  wifiMulti.addAP(secret_ssid_AP_1,secret_pass_AP_1);
  wifiMulti.addAP(secret_ssid_AP_2,secret_pass_AP_2);
  wifiMulti.addAP(secret_ssid_AP_3,secret_pass_AP_3);
  wifiMulti.addAP(secret_ssid_AP_4,secret_pass_AP_4);
  wifiMulti.addAP(secret_ssid_AP_5,secret_pass_AP_5);
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
      Serial.println("no networks found");
  } 
  else  {
      Serial.print(n);
      Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
          // Print SSID and RSSI for each network found
          Serial.print(i + 1);
          Serial.print(": ");
          Serial.print(WiFi.SSID(i));
          Serial.print(" (");
          Serial.print(WiFi.RSSI(i));
          Serial.print(")");
          Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
          delay(10);
      }
  }
  
  setup_wifi();
  //espCarCounter.setCACert(root_ca);
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(callback);

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
  display.println("Car Counter");

  Serial.println  ("Initializing Car Counter");
  Serial.print("Temperature: ");
  tempF=((rtc.getTemperature()*9/5)+32);
  Serial.print(tempF);
  Serial.println(" F");
  display.display();
  //Initialize SD Card
  initSDCard();  // Initialize SD card and ready for Read/Write
  //on reboot, get totals saved on SD Card
  getDailyTotal();  /*Daily total that is updated with every detection*/
  getShowTotal();   /*Saves Show Total*/
  getDayOfMonth();  /* Get Last Calendar Day*/ 
  getDaysRunning(); /*Needs to be reset 1st day of show*/

  if (!MDNS.begin(THIS_MQTT_CLIENT)) {
     Serial.println("Error starting mDNS");
     return;
  }
  delay(1000);
  start_MqttMillis = millis();
} /***** END SETUP ******/

void loop() {
  ElegantOTA.loop();
  DateTime now = rtc.now();
  tempF=((rtc.getTemperature()*9/5)+32);
  currentTimeMinute = now.hour()*60 + now.minute();
  if (currentTimeMinute >= showStartTime && currentTimeMinute <= showEndTime) {
     // show is running and save counts
     showTime = true;
  } 
  else {
     showTime = false;
  }

  /*****IMPORTANT***** Reset Car Counter at 5:00:00 pm ****/
  /* Only counting vehicles for show */
  if ((now.hour() == 17) && (now.minute() == 0) && (now.second() == 0))  {
      updateDailyTotal();
      updateHourlyTotals();
      totalDailyCars = 0; //Reset count to 0 before show starts
  }

  //Write Totals at 9:10:00 pm. Gate should close at 9 PM. Allow for any cars in line to come in
  if ((now.hour() == 21) && (now.minute() == 10) && (now.second() == 0)) {
    if (!hasRun) {
        WriteDailySummary();
    }
  }

  /* Reset Counts at Midnight when controller ESP32 running 24/7 */
  if ((now.hour() == 0) && (now.minute() == 0) && (now.second() == 1)) {
    dayOfMonth = now.day(); // Write new calendar day 1 second past midnight
    updateDayOfMonth();
    totalDailyCars = 0;  // reset count to 0 at 1 seond past midnight
    updateDailyTotal();
    hasRun = false; // reset flag for next day summary
    if (now.month() != 12 && now.day() != 24)  {
      // do not increment days running when closed on Christmas Eve
      if (lastdayOfMonth != dayOfMonth) {
          daysRunning++; 
          updateDaysRunning();
          getDayOfMonth();
      }
    }  
  }
 
  /* OR Reset/Update Counts when Day Changes when ESP32 is rebooted getting values from saved data */
  if (now.day() != lastdayOfMonth) {
      dayOfMonth=now.day();
      updateDayOfMonth();
      totalDailyCars = 0;
      updateDailyTotal();
      if (now.month() != 12 && now.day() != 24) {
          // do not include days running when closed on Christmas Eve
          daysRunning++; 
          updateDaysRunning();
          getDayOfMonth();
      }
  }
  
  //Save Hourly Totals
  if (now.minute()==0 && now.second()==0) {
      updateHourlyTotals();
  }

  // non-blocking WiFi and MQTT Connectivity Checks
  if (wifiMulti.run() == WL_CONNECTED) {
    // Check for MQTT connection only if wifi is connected
    if (!mqtt_client.connected()) {
        Serial.print("hour = ");
        Serial.println(currentHr12);
        Serial.println("Attempting MQTT Connection");
        MQTTreconnect();
        start_MqttMillis = millis();  
    } 
    else  {
        //keep MQTT client connected when WiFi is connected
        mqtt_client.loop();
    }
  } 
  else {
      // Reconnect WiFi if lost, non blocking
      if ((millis() - start_WiFiMillis) > wifi_connectioncheckMillis) {
          setup_wifi();
          start_WiFiMillis = millis();
      }
  }
     
  tempF=((rtc.getTemperature()*9/5)+32);

  /****** Print Day and Date 1st line  ******/
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, line1);
  //  display Day of Week
  display.print(days[now.dayOfTheWeek()]);

  //  Display Date
  display.print(" ");
  //display.print(&timeinfo, "%b");         
  display.print(months[now.month(), (DEC)]);
  display.print(" ");
  display.print(now.day(), DEC);
  display.print(", ");
  display.println(now.year(), DEC);

  // Convert 24 hour clock to 12 hours for display purposes
  currentHr24 = now.hour();
  if (currentHr24 <12)  {
     ampm ="AM";
  }
  else  {
     ampm ="PM";
  }
  if (currentHr24 > 12 ) {
     currentHr12 = now.hour() - 12;
  }
  else  {
     currentHr12 = now.hour();
  }

  /***** Display Time  and Temp Line 2 add leading 0 to Hours & display Hours *****/

  display.setTextSize(1);
  if (currentHr12 < 10) {
      display.setCursor(0, line2);
      display.print("0");
      display.print(currentHr12, DEC);
  }
  else  {
      display.setCursor(0, line2);
      display.print(currentHr12, DEC);
  }
  display.setCursor(14, line2);
  display.print(":");
  if (now.minute() < 10)  {
      display.setCursor(20, line2);
      display.print("0");
      display.print(now.minute(), DEC);
  }
  else  {
      display.setCursor(21, line2);
      display.print(now.minute(), DEC);
  }
  display.setCursor(34, line2);
  display.print(":");
  if (now.second() < 10)  {
      display.setCursor(41, line2);
      display.print("0");
      display.print(now.second(), DEC);
  }
  else  {
      display.setCursor(41, line2);
      display.print(now.second(), DEC);   
  }
  // Display AM-PM
  display.setCursor(56, line2);
  display.print(ampm); 

  // Display Temp
  display.setCursor(73, line2);
  display.print("Temp: " );
  display.println(tempF, 0);

  // Display Day Running & Grand Total
  display.setCursor(0, line3);
  display.print("Day " );
  display.print(daysRunning, 0);
  display.print(" Total: ");
  display.println(totalShowCars);

  // Display Car Count
  display.setTextSize(2);
  display.setCursor(0, line5);
  display.print("Cars: ");
  display.println(totalDailyCars);

  display.display();  // write to display

  detectCar();  // Looking for car in detection zone

    //***** PLAY PATTERN WHEN NO CARS PRESENT *****/   
    if(millis() - noCarTimer >= 30000 && secondBeamState == 0) {
        playPattern(); 
    }

  //Added to kepp mqtt connection alive 10/11/24 gal
  if (mqtt_client.connected()) {
        publishQueuedMessages();  // Publish any messages that were queued during a disconnected state
    }
  if  ((millis() - start_MqttMillis) > (mqttKeepAlive*1000)) {
      KeepMqttAlive();
  }

} /***** Repeat Loop *****/