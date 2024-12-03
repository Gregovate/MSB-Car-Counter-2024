/*
Car counter Interface to MQTT by Greg Liebig gliebig@sheboyganlights.org
Initial Build 12/5/2023 12:15 pm
Changed time format YYYY-MM-DD hh:mm:ss 12/13/23

Changelog
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

// ******************** CONSTANTS *******************
#define firstBeamPin 33
#define secondBeamPin 32
#define redArchPin 25
#define greenArchPin 26
#define PIN_SPI_CS 5 // The ESP32 pin GPIO5
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// #define MQTT_KEEPALIVE 30 //removed 10/16/24
#define FWVersion "24.12.3.1" // Firmware Version
#define OTA_Title "Car Counter" // OTA Title
unsigned int carDetectMillis = 750; // minimum millis for secondBeam to be broken needed to detect a car
unsigned int showStartTime = 16*60 + 55; // Show (counting) starts at 4:55 pm
unsigned int showEndTime =  21*60 + 10;  // Show (counting) ends at 9:10 pm 
// **************************************************



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

//#include <DS3231.h> & Setup display variables
#define DS3231_I2C_ADDRESS 0x68
RTC_DS3231 rtc;

const int line1 =0;
const int line2 =9;
const int line3 = 19;
const int line4 = 35;
const int line5 = 50;

//Create Multiple WIFI Object
WiFiMulti wifiMulti;
//WiFiClientSecure espCarCounter;
WiFiClient espCarCounter;
PubSubClient mqtt_client(espCarCounter);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
int value = 0;

char mqtt_server[] = mqtt_Server;
char mqtt_username[] = mqtt_UserName;
char mqtt_password[] = mqtt_Password;
const int mqtt_port = mqtt_Port;
bool mqtt_connected = false;
bool wifi_connected = false;
bool showTime = false;
int wifi_connect_attempts = 5;
bool hasRun = false;


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
#define MQTT_PUB_SECOND_BEAM_SENSOR_STATE "msb/traffic/CarCounter/secondBeamSensorState"
#define MQTT_PUB_TTP "msb/traffic/CarCounter/TTP"
#define MQTT_PUB_DAYSRUNNING "msb/traffic/CarCounter/DaysRunning"

// Subscribing Topics (to reset values)
//#define MQTT_SUB_TOPIC0  "msb/traffic/CarCounter/EnterTotal"
#define MQTT_SUB_TOPIC1  "msb/traffic/CarCounter/resetDailyCount"
#define MQTT_SUB_TOPIC2  "msb/traffic/CarCounter/resetShowCount"
#define MQTT_SUB_TOPIC3  "msb/traffic/CarCounter/resetDayOfMonth"
#define MQTT_SUB_TOPIC4  "msb/traffic/CarCounter/resetDaysRunning"
#define MQTT_SUB_TOPIC5  "msb/traffic/CarCounter/carCounterTimeout"


//const uint32_t connectTimeoutMs = 10000;
uint16_t connectTimeOutPerAP=5000;
const char* ampm ="AM";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;
const int   daylightOffset_sec = 3600;
float tempF;

/***** GLOBAL VARIABLES *****/
unsigned int DayOfMonth; // Current Calendar day
unsigned int currentHr12; //Current Hour 12 Hour format
unsigned int currentHr24; //Current Hour 24 Hour Format
unsigned int currentMin;
unsigned int currentSec;
unsigned int lastDayOfMonth; // Pervious day's calendar day used to reset running days
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
int carPresentFlag; // Flag used to detect car in detection zone
int carCounterTimeout = 60000; // default time for car counter alarm in millis

unsigned long currentMillis; // Comparrison time holder
unsigned long carDetectedMillis;  // Grab the ime when sensor 1st trips
unsigned long firstBeamTripTime; // millis when first Beam tripped
unsigned long secondBeamTripTime; // millis when second Beam tripped
unsigned long TimeToPassMillis; // millis for car to pass
unsigned long wifi_connectioncheckMillis = 5000; // check for connection every 5 sec
unsigned long mqtt_connectionCheckMillis = 20000; // check for connection
unsigned long start_MqttMillis; // for Keep Alive Timer
unsigned long start_WiFiMillis; // for keep Alive Timer

int firstBeamState;  // Holds the current state of the FIRST IR receiver/Beam
int secondBeamState;  // Holds the current state of the SECOND IR receiver/Beam
int prevFirstBeamState = -1; // Holds the previous state of the FIRST IR receiver/Beam
int prevSecondBeamState = -1; // Holds the previous state of the SECOND IR receiver/Beam

unsigned long bothBeamHighMillis = 0;
int BeamTrippedCount = 0;
unsigned long noCarTimer = 0;
unsigned int totalCars;
int displayMode = 0;
unsigned long displayModeMillis = 0;
unsigned long dayMillis = 0;
int alternateColorMode = 0;
int patternMode = 0;
unsigned long patternModeMillis = 0;

File myFile; //used to write files to SD Card

// **********FILE NAMES FOR SD CARD *********
const String fileName1 = "/EnterTotal.txt"; // DailyTot.txt file to store daily counts in the event of a Failure
const String fileName2 = "/ShowTotal.txt";  // ShowTot.txt file to store season total counts
const String fileName3 = "/DayOfMonth.txt"; // DayOfMonth.txt file to store current day number
const String fileName4 = "/RunDays.txt"; // RunDays.txt file to store days since open
const String fileName5 = "/EnterSummary.csv"; // EnterSummary.csv Stores Daily Totals by Hour and total
const String fileName6 = "/EnterLog.csv"; // EnterLog.csv file to store all car counts for season (was MASTER.CSV)
//const String fileName7 = "/ShowSummary.csv"; // Shows summary of counts during show (4:55pm to 9:10pm)

char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
int dayHour[24]; // Array for Daily total cars per hour
int showHour[5]; // Array for Show total cars per hour starting at 5:00 pm and ending at 9:10 pm

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);;

void setup_wifi()
 {
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
 // print the SSID of the network you're attached to:
  display.setCursor(0, line1);
  display.print("SSID: ");
  display.println(WiFi.SSID());
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

void publishMQTT(const char *topic, const String &message) {
    if (mqtt_client.connected()) {
        mqtt_client.publish(topic, message.c_str());
    } else {
        Serial.printf("MQTT not connected. Failed to publish: %s -> %s\n", topic, message.c_str());
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
} // end SetLocalTime

// =========== GET SAVED SETUP FROM SD CARD ==========
void getDailyTotal()   // open DAILYTOT.txt to get initial dailyTotal value
{
   myFile = SD.open(fileName1,FILE_READ);
   if (myFile)
   {
     while (myFile.available()) {
     totalDailyCars = myFile.parseInt(); // read total
     Serial.print(" Daily cars from file = ");
     Serial.println(totalDailyCars);
    }
    myFile.close();
    publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
  }
  else
  {
    Serial.print("SD Card: Cannot open the file: ");
    Serial.println(fileName1);
  }
} // end getDailyTotal

void getShowTotal()     // open ShowTot.txt to get total Cars for season
{
  myFile = SD.open(fileName2,FILE_READ);
  if (myFile)
  {
    while (myFile.available())
    {
      totalShowCars = myFile.parseInt(); // read total
      Serial.print(" Total Show cars from file = ");
      Serial.println(totalShowCars);
    }
    myFile.close();
    publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
  }
  else
  {
    Serial.print("SD Card: Cannot open the file: ");
    Serial.println(fileName2);
  }
}  // end getShowTotal

void getDayOfMonth()  // get the last calendar day used for reset daily counts)
{
   myFile = SD.open(fileName3,FILE_READ);
   if (myFile)
   {
     while (myFile.available()) {
     lastDayOfMonth = myFile.parseInt(); // read day Number
     Serial.print(" Calendar Day = ");
     Serial.println(lastDayOfMonth);
     publishMQTT(MQTT_PUB_DAYOFMONTH, String(lastDayOfMonth));
     }
   myFile.close();
   }
   else
   {
    Serial.print("SD Card: Cannot open the file: ");
    Serial.println(fileName3);
   }
}

void getDaysRunning()   // Days the show has been running)
{
  myFile = SD.open(fileName4,FILE_READ);
  if (myFile)
  {
    while (myFile.available()) {
    daysRunning = myFile.parseInt(); // read day Number
    Serial.print(" Days Running = ");
    Serial.println(daysRunning);
    }
    myFile.close();
    publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
  }
  else
  {
    Serial.print("SD Card: Cannot open the file: ");
    Serial.println(fileName4);
  }
} 

/***** UPDATE TOTALS TO SD CARD *****/
void updateHourlyTotals()
{
  char hourPad[6];
  sprintf(hourPad,"%02d", currentHr24);
  dayHour[currentHr24]= totalDailyCars; //write daily total cars to array each hour
  strcpy (topicBase, topic_base_path);
  strcat (topicBase, "/daily/hour/");
  //strcat (topicBase, String(currentHr24).c_str());
  strcat (topicBase, hourPad);
      publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
}


void updateDailyTotal()
{
  myFile = SD.open(fileName1,FILE_WRITE);
  if (myFile)
  {  // check for an open failure
     myFile.print(totalDailyCars);
     myFile.close();
     publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
  }
  else
  {
    Serial.print(F("SD Card: Cannot open the file: "));
    Serial.println(fileName1);
  }
  publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
}

void updateShowTotal()  /* -----Increment the grand total cars file for season  CHECK ----- */
{  
   myFile = SD.open(fileName2,FILE_WRITE);
   if (myFile) 
   {
      myFile.print(totalShowCars); // only count cars between 4:55 pm and 9:10 pm
      myFile.close();
      Serial.print(F("Updating Show Total "));
      Serial.print(totalShowCars);
      Serial.print(F(" Car # "));
  }
  else
  {
    Serial.print(F("SD Card: Cannot open the file: "));
    Serial.println(fileName2);
  }
  publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));  
}

void updateDayOfMonth()  /* -----write calendar day 1 seond past midnight to file ----- */
{
   myFile = SD.open(fileName3,FILE_WRITE);
   if (myFile)
   {
      myFile.print(DayOfMonth);
      myFile.close();
      
   }
   else
   {
    Serial.print(F("SD Card: Cannot open the file: "));
    Serial.println(fileName3);
   }
   publishMQTT(MQTT_PUB_DAYOFMONTH, String(DayOfMonth));
}

void updateDaysRunning()
{
  myFile = SD.open(fileName4,FILE_WRITE);
  if (myFile) // check for an open failure
  {
    myFile.print(daysRunning);
    myFile.close();
  }
  else
  {
    Serial.print(F("SD Card: Cannot open the file: "));
    Serial.println(fileName4);
  }
  publishMQTT(MQTT_PUB_DAYSRUNNING, String(daysRunning));
}

void WriteDailySummary() // Write totals daily at end of show (EOS Totals)
{
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
  if (myFile) 
  {
    myFile.print(now.toString(buf2));
    myFile.print(", "); 
    for (int i = 16; i<=21; i++)
      {
        myFile.print(dayHour[i]);
        myFile.print(", ");
      }    
    myFile.println(tempF);
    myFile.close();
    Serial.println(F(" = Daily Summary Recorded SD Card."));
  }
  else
  {
    Serial.print(F("SD Card: Cannot open the file: "));
    Serial.println(fileName5);
  }
    // Publish Totals
  publishMQTT(MQTT_PUB_TEMP, String(tempF));
  publishMQTT(MQTT_PUB_TIME, now.toString(buf2));
  publishMQTT(MQTT_PUB_CARS_18, String(dayHour[18]));
  publishMQTT(MQTT_PUB_CARS_19, String(dayHour[19]));
  publishMQTT(MQTT_PUB_CARS_20, String(dayHour[20]));
  publishMQTT(MQTT_PUB_CARS_21, String(dayHour[21]));
  publishMQTT(MQTT_PUB_ENTER_TOTAL, String(totalDailyCars));
  publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars));
  publishMQTT(MQTT_PUB_HELLO, "Enter Summary File Updated");
  hasRun = true;
}
/***** END OF FILE UPDATES *****/

/***** IDLE STUFF  *****/
void playPattern() // Flash an alternating pattern on the arches (called if a car hasn't been detected for over 30 seconds)
{ 
  if (patternMode == 0 && (millis() - patternModeMillis) >= 500)
  {
    digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
    digitalWrite(greenArchPin, HIGH); // Turn Green Arch Off
    patternMode++;
    patternModeMillis = millis();
  }
  if (patternMode == 1 && (millis() - patternModeMillis) >= 500)
  {
    digitalWrite(redArchPin, LOW); // Turn Red Arch On
    digitalWrite(greenArchPin, HIGH); // Turn Green Arch Off
    patternMode++;
    patternModeMillis = millis();
  }
  if (patternMode == 2 && (millis() - patternModeMillis) >= 500)
  {
    digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
    digitalWrite(greenArchPin, LOW); // Turn Green Arch On
    patternMode++;
    patternModeMillis = millis();
  }
  if (patternMode == 3 && (millis() - patternModeMillis) >= 500)
  {
     digitalWrite(redArchPin, LOW); // Turn Red Arch On
     digitalWrite(greenArchPin, LOW); // Turn Green Arch On
     patternMode++;
     patternModeMillis = millis();
  }
  if (patternMode == 4 && (millis() - patternModeMillis) >= 500)
  {
     digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
     digitalWrite(greenArchPin, HIGH); // Turn Green Arch Off
     patternMode++;
     patternModeMillis = millis();
  }
  if (patternMode == 5 && (millis() - patternModeMillis) >= 500)
  {
     digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
     digitalWrite(greenArchPin, LOW); // Turn Green Arch On
     patternMode++;
     patternModeMillis = millis();
  }
  if (patternMode == 6 && (millis() - patternModeMillis) >= 500)
  {
     digitalWrite(redArchPin, LOW); // Turn Red Arch On
     digitalWrite(greenArchPin, HIGH); // Turn Green Arch Off
     patternMode++;
     patternModeMillis = millis();
  }
  if (patternMode == 7 && (millis() - patternModeMillis) >= 500)
  {
     digitalWrite(redArchPin, LOW); // Turn Red Arch On
     digitalWrite(greenArchPin, LOW); // Turn Green Arch On
     patternMode = 0;
     patternModeMillis = millis();
  }
}

void beamCarDetect() // If a car is counted, then increment the counter by 1 and add an entry to the Enterlog.csv log file on the SD card
{
  /* COUNT SUCCESS */
  digitalWrite(redArchPin, HIGH);
  digitalWrite(greenArchPin, LOW);
  noCarTimer = millis();

/*------Append to log file*/
  DateTime now = rtc.now();
  char buf2[] = "YYYY-MM-DD hh:mm:ss";
  Serial.print(now.toString(buf2));
  Serial.print(", Temp = ");
  Serial.print(tempF);
  Serial.print(", ");
  totalDailyCars ++;   // increase Count for Every car going through car counter regardless of time
  updateDailyTotal();  // Update Daily Total on SD Card to retain numbers with reboot
  if (showTime == true)
  {
    totalShowCars ++;   // increase Show Count only when show is open
    updateShowTotal();  // update show total count in event of power failure during show hours
  }
  Serial.print(totalDailyCars) ;  
  // open file for writing Car Data
  myFile = SD.open(fileName6, FILE_APPEND); //Enterlog.csv
  if (myFile)
  {
    myFile.print(now.toString(buf2));
    myFile.print(", ");
    myFile.print (TimeToPassMillis) ; 
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
    publishMQTT(MQTT_PUB_SECOND_BEAM_SENSOR_STATE, String(secondBeamState));
    publishMQTT(MQTT_PUB_TTP, String(TimeToPassMillis));
    start_MqttMillis = millis();
  }
  else
  {
    Serial.print(F("SD Card: Cannot open the file: "));
    Serial.println(fileName6);
  }

} /* END Beam Car Detect*/

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

// Init microSD card
void initSDCard()
{
  if(!SD.begin(PIN_SPI_CS)){
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

  if(cardType == CARD_NONE){
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
void callback(char* topic, byte* payload, unsigned int length)
{
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

  /* Topic used to manually reset Enter Daily Cars */
  if (strcmp(topic, MQTT_SUB_TOPIC1) == 0)
  {
    totalDailyCars = atoi((char *)payload);
    updateDailyTotal();
    Serial.println(F(" Car Counter Updated"));
    publishMQTT(MQTT_PUB_HELLO, "Daily Total Updated");
  }

  /* Topic used to manually reset Total Show Cars */
  if (strcmp(topic, MQTT_SUB_TOPIC2) == 0)
  {
    totalShowCars = atoi((char *)payload);
    updateShowTotal();
    Serial.println(F(" Show Counter Updated"));
    publishMQTT(MQTT_PUB_HELLO, "Show Counter Updated");
  }  

  /* Topic used to manually reset Calendar Day */
  if (strcmp(topic, MQTT_SUB_TOPIC3) == 0)
  {
    DayOfMonth = atoi((char *)payload);
    updateDayOfMonth();
    Serial.println(F(" Calendar Day of Month Updated"));
    publishMQTT(MQTT_PUB_HELLO, "Calendar Day Updated");
  }  

  /* Topic used to manually reset Days Running */
  if (strcmp(topic, MQTT_SUB_TOPIC4) == 0)
  {
    daysRunning = atoi((char *)payload);
    updateDaysRunning();
    Serial.println(F(" Days Running Updated"));
    publishMQTT(MQTT_PUB_HELLO, "Days Running Updated");
  }  
 
  // Topic used to change car counter timeout  
  if (strcmp(topic, MQTT_SUB_TOPIC5) == 0)
  {
    carCounterTimeout = atoi((char *)payload);
    Serial.println(F(" Car Counter Alarm Timer Updated"));
    publishMQTT(MQTT_PUB_HELLO, "Car Counter Timeout Updated");
  }  
 

} /***** END OF CALLBACK TOPICS *****/

/******  BEGIN SETUP ******/
void setup()
{
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
  
  //Initialize SD Card
  initSDCard();  // Initialize SD card and ready for Read/Write

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
  if (n == 0)
  {
    Serial.println("no networks found");
  } 
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
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

  //on reboot, get totals saved on SD Card
  getDailyTotal();  /*Daily total that is updated with every detection*/
  getShowTotal();   /*Saves Show Total*/
  getDayOfMonth();  /* Get Last Calendar Day*/ 
  getDaysRunning(); /*Needs to be reset 1st day of show*/

   // Read Digital Pin States for debugging
  firstBeamState = digitalRead (firstBeamPin); //Read the current state of the FIRST IR beam receiver/Beam
  secondBeamState = digitalRead (secondBeamPin);
  Serial.print("firstBeam State = ");
  Serial.print(firstBeamState);
  Serial.print(" secondBeamState = ");
  Serial.println(secondBeamState);

  if (!MDNS.begin(THIS_MQTT_CLIENT))
  {
     Serial.println("Error starting mDNS");
     return;
  }
  delay(1000);
  start_MqttMillis = millis();
} /***** END SETUP ******/

void loop()
{
  ElegantOTA.loop();
  DateTime now = rtc.now();
  tempF=((rtc.getTemperature()*9/5)+32);
  currentMillis = millis();
  currentTimeMinute = now.hour()*60 + now.minute();
  if (currentTimeMinute >= showStartTime && currentTimeMinute <= showEndTime)
  {
    showTime = true;
  } // show is running and save counts
  else
  {
    showTime = false;
  }
  /*****IMPORTANT***** Reset Car Counter at 5:00:00 pm ****/
  /* Only counting vehicles for show */
  if ((now.hour() == 17) && (now.minute() == 0) && (now.second() == 0))  
  {
    updateDailyTotal();
    updateHourlyTotals();
    totalDailyCars = 0; //Reset count to 0 before show starts
  }
    //Write Totals at 9:10:00 pm. Gate should close at 9 PM. Allow for any cars in line to come in
    if ((now.hour() == 21) && (now.minute() == 10) && (now.second() == 0))
    {
      if (!hasRun)
      {
        WriteDailySummary();
      }
  }

  /* Reset Counts at Midnight when controller running 24/7 */
  if ((now.hour() == 0) && (now.minute() == 0) && (now.second() == 1))
  {
    DayOfMonth = now.day(); // Write new calendar day 1 second past midnight
    updateDayOfMonth();
    totalDailyCars = 0;  // reset count to 0 at 1 seond past midnight
    updateDailyTotal();
    hasRun = false; // reset flag for next day summary
    if (now.month() != 12 && now.day() != 24) // do not increment days running when closed on Christmas Eve
    {
      if (lastDayOfMonth != DayOfMonth)
      {
        daysRunning++; 
        updateDaysRunning();
        getDayOfMonth();
      }
    }  
  }
 
  /* OR Reset/Update Counts when Day Changes on reboot getting values from saved data */
  if (now.day() != lastDayOfMonth)
  {
    DayOfMonth=now.day();
    updateDayOfMonth();
    totalDailyCars = 0;
    updateDailyTotal();
    if (now.month() != 12 && now.day() != 24) // do not include days running when closed on Christmas Eve
    {
      daysRunning++; 
      updateDaysRunning();
      getDayOfMonth();
    }
  }
  
  //Save Hourly Totals
  if (now.minute()==0 && now.second()==0)
  {
    updateHourlyTotals();
  }

  // non-blocking WiFi and MQTT Connectivity Checks
  if (wifiMulti.run() == WL_CONNECTED)
  {
    // Check for MQTT connection only if wifi is connected
    if (!mqtt_client.connected())
    {
      Serial.print("hour = ");
      Serial.println(currentHr12);
      Serial.println("Attempting MQTT Connection");
      MQTTreconnect();
      start_MqttMillis = millis();  
    } 
    else
    {
      //keep MQTT client connected when WiFi is connected
      mqtt_client.loop();
    }
  } 
  else
  {
    // Reconnect WiFi if lost, non blocking
    if ((currentMillis - start_WiFiMillis) > wifi_connectioncheckMillis)
    {
      setup_wifi();
      start_WiFiMillis = currentMillis;
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
  if (currentHr24 <12)
  {
      ampm ="AM";
  }
  else
  {
      ampm ="PM";
  }
  if (currentHr24 > 12 )
  {
      currentHr12 = now.hour() - 12;
  }
  else
  {
      currentHr12 = now.hour();
  }

  /***** Display Time  and Temp Line 2 add leading 0 to Hours & display Hours *****/

  display.setTextSize(1);
  if (currentHr12 < 10)
  {
    display.setCursor(0, line2);
    display.print("0");
    display.print(currentHr12, DEC);
  }
  else
  {
    display.setCursor(0, line2);
    display.print(currentHr12, DEC);
  }
  display.setCursor(14, line2);
  display.print(":");
  if (now.minute() < 10)
  {
    display.setCursor(20, line2);
    display.print("0");
    display.print(now.minute(), DEC);
  }
  else
  {
    display.setCursor(21, line2);
    display.print(now.minute(), DEC);
  }
  display.setCursor(34, line2);
  display.print(":");
  if (now.second() < 10)
  {
    display.setCursor(41, line2);
    display.print("0");
    display.print(now.second(), DEC);
  }
  else
  {
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

  display.display();

  /* LOOP PIN STATE FOR DEBUG 
  digitalWrite(redArchPin, HIGH);
  digitalWrite(greenArchPin, HIGH);
  Serial.print("firstBeam State = ");
  Serial.print(firstBeamState);
  Serial.print(" secondBeamState = ");
  Serial.println(secondBeamState);
  */

  firstBeamState = digitalRead (firstBeamPin); //Read the current state of the FIRST IR beam receiver/Beam Tripped = 1
  secondBeamState = digitalRead (secondBeamPin); //Read the current state of the SECOND IR beam receiver/Beam Tripped =1

  //***** DETECTING CARS *****/
  /* Sense Vehicle & Count Cars Entering
  Both sensors HIGH when vehicle sensed, Normally both normally open (LOW)
  Both Sensors need to be active to start sensing vehicle for detect millis
  Then Beam confirms vehicle is present and then counts car after vehicle passes
  
  if (firstBeamState != prevFirstBeamState && firstBeamState == 1) // if 1st beam switches to HIGH set timer
  {
    firstBeamTripTime = millis();
  }
  if (secondBeamState != prevSecondBeamState && secondBeamState == 1) // if 2nd beam switches to High set Timer
  {
    secondBeamTripTime = millis(); // double check. may need a way to reset if there is a bounce 11/3/24
  }

  if (firstBeamState == 1 && secondBeamState == 1) /* Both Beams Blocked */
  {
    if (millis()-secondBeamTripTime  >= carDetectMillis) // if second beam is blocked for x millis
    {
      carPresentFlag = 1;  // set Car Present Flag
      carDetectedMillis = firstBeamTripTime;    // if a car is in the detection zone, freeze time when first detected
      publishMQTT(MQTT_PUB_SECOND_BEAM_SENSOR_STATE, String(secondBeamState));  // publishes beamSensor State goes HIGH
    }
    else
    {
     firstBeamState = digitalRead (firstBeamPin);
     secondBeamState = digitalRead (secondBeamPin);
       if (secondBeamState != prevSecondBeamState && secondBeamState == 0)
       {
         secondBeamTripTime = millis(); // reset second beam trip time if it bounces 11/16/24
       }
     carPresentFlag = 0;
    }
  
    /* DEBUG HELPERS
    DateTime now = rtc.now();
    char buf3[] = "YYYY-MM-DD hh:mm:ss"; //time of day when detector was tripped
    Serial.print("Detector Triggered = ");
    Serial.print(now.toString(buf3));
    Serial.print(", secondBeamSensorState = ");
    Serial.print(secondBeamState);
    Serial.print(", firstBeamTripTime = ");
    Serial.print(firstBeamTripTime);
    Serial.print(", secondBeamTripTime = ");
    Serial.print(secondBeamTripTime);
    Serial.print(millis() - carDetectedMillis);
    Serial.print(", Car Number Being Counted = ");         
    Serial.println (totalDailyCars+1) ;  //add 1 to total daily cars so car being detected is synced
    */

    while (carPresentFlag == 1) // Car in detection zone, Turn on RED arch
    {
      secondBeamState = digitalRead(secondBeamPin); 
      digitalWrite(redArchPin, HIGH); // Turn on Red Arch
      digitalWrite(greenArchPin, LOW); // Turn Off Green Arch
      TimeToPassMillis = millis() - carDetectedMillis; // Record time to Pass  

      // Added to detect car counter problem with blocked sensor 
      if (TimeToPassMillis == carCounterTimeout ) // default time for car counter alarm in millis
      {
        publishMQTT(MQTT_PUB_HELLO, "Check Car Counter!");
        /*
        Serial.print(TimeToPassMillis);
        Serial.print("\t");
        Serial.print(carCounterTimeout);
        Serial.print("\t");
        Serial.println(carPresentFlag);
        */
        carPresentFlag = 0;
      }    
      
      if (secondBeamState == 0)  /* when second sensor goes low, Car has passed */
      {
        TimeToPassMillis = millis() - carDetectedMillis; // Record time to Pass
        carPresentFlag = 0; // Car has exited detection zone. Turn On Green Arch
        digitalWrite(redArchPin, LOW); // Turn off Red Arch
        digitalWrite(greenArchPin, HIGH); // Turn On Green Arch
        beamCarDetect(); //count car and update files
      } // end of car passed check
      mqtt_client.loop(); // Keep MQTT Active when car takes long time to pass
    } // end of Car in detection zone (while loop)
  } /* End if when both Beam Sensors are HIGH */
  /***** END OF CAR DETECTION *****/
  
  /*--------- Reset the counter if the beam is not broken --------- */    
  if (secondBeamState == LOW)  //Check to see if the beam is not broken, this prevents the green arch from never turning off)
  {
    if(millis() - noCarTimer >= 30000) // If the beam hasn't been broken by a vehicle for over 30 seconds then start a pattern on the arches.
    {
      playPattern(); //***** PLAY PATTERN WHEN NO CARS PRESENT *****/
    }
    else
    {
      // reset arches and ready to count next car
      digitalWrite(redArchPin, LOW);// Turn Red Arch Off
      digitalWrite(greenArchPin, HIGH); // Turn Green Arch On
    }
  }
  //Added to kepp mqtt connection alive 10/11/24 gal
  if  ((millis() - start_MqttMillis) > (mqttKeepAlive*1000))
  {
      KeepMqttAlive();
  }
  prevFirstBeamState = firstBeamState;
  prevSecondBeamState = secondBeamState;
} /***** Repeat Loop *****/