/*
Car counter Interface to MQTT by Greg Liebig gliebig@sheboyganlights.org
Initial Build 12/5/2023 12:15 pm
Changed time format YYYY-MM-DD hh:mm:ss 12/13/23

FW Version
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
//#include "FS.h"
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
#define firstDetectorPin 33
#define secondDetectorPin 32
#define redArchPin 25
#define greenArchPin 26
#define PIN_SPI_CS 5 // The ESP32 pin GPIO5
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// #define MQTT_KEEPALIVE 30 //removed 10/16/24
#define FWVersion "24.10.21.1" // Firmware Version
#define OTA_Title "Gate Counter" // OTA Title
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
RTC_DS3231 rtc;

const int line1 =0;
const int line2 =9;
const int line3 = 20;
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
int wifi_connect_attempts = 5;

//***** MQTT DEFINITIONS *****/
#define THIS_MQTT_CLIENT "espCarCounter" // Look at line 90 and set variable for WiFi Client secure & PubSubCLient 12/23/23
#define MQTT_PUB_TOPIC0  "msb/traffic/enter/hello"
#define MQTT_PUB_TOPIC1  "msb/traffic/enter/temp"
#define MQTT_PUB_TOPIC2  "msb/traffic/enter/time"
#define MQTT_PUB_TOPIC3  "msb/traffic/enter/count"



//const uint32_t connectTimeoutMs = 10000;
uint16_t connectTimeOutPerAP=5000;
const char* ampm ="AM";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;
const int   daylightOffset_sec = 3600;
int16_t tempF;

unsigned int currentDay = 0;
unsigned int currentHour = 0;
unsigned int currentMin = 0;
unsigned int lastCalDay = 0;
unsigned int totalDailyCars = 0;
unsigned int totalShowCars = 0;
int connectionAttempts = 5;
unsigned int carsHr1 =0;
unsigned int carsHr2 =0;
unsigned int carsHr3 =0;
unsigned int carsHr4 =0;


unsigned long debounceMillis = 12000; // Time required for my truck to pass totally
unsigned long nocarMillis = 3500; // Time required for High Pin to stay high to reset car in detection zone
unsigned long highMillis = 0; //Grab the time when the vehicle sensor is high
unsigned long previousMillis; // Last time sendor pin changed state
unsigned long currentMillis; // Comparrison time holder
unsigned long carDetectedMillis;  // Grab the ime when sensor 1st trips
unsigned long wifi_lastReconnectAttemptMillis;
unsigned long wifi_connectioncheckMillis = 5000; // check for connection every 5 sec
unsigned long mqtt_lastReconnectAttemptMillis;
unsigned long mqtt_connectionCheckMillis = 5000;
unsigned long nowwifi;
unsigned long nowmqtt;
int firstDetectorState = 0;  // Holds the current state of the FIRST IR receiver/detector
int secondDetectorState = 0;  // Holds the current state of the SECOND IR receiver/detector

int previousFirstDetectorState = 0; // Holds the previous state of the FIRST IR receiver/detector
int previousSecondDetectorState =0; // Holds the previous state of the SECOND IR receiver/detector


//int detectorState;  // was used for interface between ESP32 and UNO 2023
//int lastdetectorState = 0; // was used for interface between ESP32 and UNO 2023

unsigned long detectorMillis = 0;
int detectorTrippedCount = 0;
unsigned long noCarTimer = 0;
unsigned int totalCars;
unsigned int totalDailyCarstal;  
int displayMode = 0;
unsigned long displayModeMillis = 0;
unsigned long dayMillis = 0;
int daysRunning;
int alternateColorMode = 0;
int patternMode = 0;
unsigned long patternModeMillis = 0;


File myFile; //used to write files to SD Card
File myFile2; // writes hourly car data to file

// **********FILE NAMES FOR SD CARD *********
String fileName1 = "/DailyTot.txt"; // DailyTot.txt file to store daily counts in the event of a Failure
String fileName2 = "/ShowTot.txt";  // ShowTot.txt file to store season total counts
String fileName3 = "/CalDay.txt"; // CalDay.txt file to store current day number
String fileName4 = "/RunDays.txt"; // RunDays.txt file to store days since open
String fileName5 = "/CarLog.csv"; // CarLog.csv file to store all car counts for season (was MASTER.CSV)
String fileName6 = "/DailySummary.csv"; // DailySummary.csv Stores Daily Totals by Hour and total

char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);;




void setup_wifi() {
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

  delay(5000);
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}


void reconnect() {
  // Loop until we’re reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection… ");
    String clientId = "ESP32ClientCar";
    // Attempt to connect
    if (mqtt_client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0,line5);
      display.println("MQTT Connect");
      display.display();
      Serial.println("connected!");
      Serial.println("Waiting for Car...");
      // Once connected, publish an announcement…
      mqtt_client.publish(MQTT_PUB_TOPIC0, "Hello from Car Counter!");
      // … and resubscribe
      mqtt_client.subscribe(MQTT_PUB_TOPIC0);
    } else {
      Serial.print("failed, rc = ");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0,line4);
      display.println("MQTT Error");
      display.display();
    }
  }
}



void SetLocalTime() {
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
}

// =========== GET SETUP COUNTS FROM SD CARD ==========
void getShowTotal()  {   // open ShowTot.txt to get totalCars for season
    myFile = SD.open(fileName2,FILE_READ);
    if (myFile) {
      while (myFile.available()) {
      totalShowCars = myFile.parseInt(); // read total
      Serial.print(" Total cars from file = ");
      Serial.println(totalShowCars);
    }
    myFile.close();
    }  else {
        Serial.print(F("SD Card: Cannot open the file TOTAL.TXT"));
    }
}

void getInitialDailyTotal()  { // open DAILYTOT.txt to get initial dailyTotal value
    myFile = SD.open(fileName1,FILE_READ);
    if (myFile) {
      while (myFile.available()) {
      totalDailyCars = myFile.parseInt(); // read total
      Serial.print(" Daily cars from file = ");
      Serial.println(totalDailyCars);
    }
    myFile.close();
    }  else {
       Serial.print(F("SD Card: Cannot open the file DailyTotal.TXT"));
    }
}

void getInitialDayRunning() {  // Days the show has been running)
    myFile = SD.open(fileName4,FILE_READ);
    if (myFile) {
      while (myFile.available()) {
      daysRunning = myFile.parseInt(); // read day Number
      Serial.print(" Days Running = ");
      Serial.println(daysRunning);
    }
    myFile.close();
    }  else {
       Serial.print(F("SD Card: Cannot open the file Running.TXT"));
    }
} 

void getLastDayRunning() {  // get the last calendar day used for reset daily counts)
    myFile = SD.open(fileName3,FILE_READ);
    if (myFile) {
      while (myFile.available()) {
       lastCalDay = myFile.parseInt(); // read day Number
      Serial.print(" Calendar Day = ");
      Serial.println(lastCalDay);
    }
    myFile.close();
    }  else {
       Serial.print(F("SD Card: Cannot open the file Running.TXT"));
    }
} 

void HourlyTotals()  {
  if (currentHour == 18){
    carsHr1 = totalDailyCars;
  }
  if (currentHour == 19){
    carsHr2 = totalDailyCars-carsHr1;
  }
  if (currentHour == 20){
    carsHr3 = totalDailyCars-(carsHr1+carsHr2);
  }
  if (currentHour == 21){
    carsHr4 = totalDailyCars;
  }
}

void WriteTotals(){
  DateTime now = rtc.now();
  char buf2[] = "YYYY-MM-DD hh:mm:ss";
  Serial.print(now.toString(buf2));
  Serial.print(", Temp = ");
  Serial.print(tempF);
  Serial.print(", ");
  totalDailyCars ++;     
  Serial.print(totalDailyCars) ;  
  // open file for writing Car Data
  myFile2 = SD.open(fileName6, FILE_APPEND);
  if (myFile) {
  myFile2.print(now.toString(buf2));
  myFile2.print(", ");
  myFile2.print (tempF); 
  myFile2.print(", "); 
  myFile2.print (carsHr1) ; 
  myFile2.print(", ");
  myFile2.println(carsHr2);
  myFile2.print(", ");
  myFile2.println(carsHr3);
  myFile2.print(", ");
  myFile2.println(carsHr4);
  myFile2.print(", ");
  myFile2.println(totalDailyCars);
  myFile2.close();
  Serial.println(F(" = Daily Summary Recorded SD Card."));
    // Publish Totals
    mqtt_client.publish(MQTT_PUB_TOPIC1, String(tempF).c_str());
    mqtt_client.publish(MQTT_PUB_TOPIC2, now.toString(buf2));
    mqtt_client.publish(MQTT_PUB_TOPIC3, String(totalDailyCars).c_str());
  } else {
  Serial.print(F("SD Card: Issue encountered while attempting to open the file CarCount.csv"));
  }
}

void updateShowTotal() { /* -----Increment the grand total cars file ----- */
   myFile = SD.open(fileName2,FILE_WRITE);
  if (myFile) {
    myFile.print(totalShowCars);
    myFile.close();
  } else {
    Serial.print(F("SD Card: Cannot open the file ShowTot.TXT"));
  }
}


//***** UPDATE TOTALS ******/
 void updateDailyTotal()  {
   myFile = SD.open(fileName1,FILE_WRITE);
  if (myFile) {     // check for an open failure
    myFile.print(totalDailyCars);
    myFile.close();
  }  else  {
    Serial.print(F("SD Card: Cannot open the file:  DailyTot.txt"));
  } 
}

void incrementDaysRunning()  {
  myFile = SD.open(fileName4,FILE_WRITE);
  if (myFile) {     // check for an open failure
    myFile.print(daysRunning++);
    myFile.close();
  }  else  {
    Serial.print(F("SD Card: Cannot open the file RunDays.txt"));
  }
}


//***** DISPLAY ROUTINES *****/

void displayGrandTotal()  {
    display.clearDisplay();
    display.setCursor(0,line3); //Start at character 4 on line 0
    display.print("Total Cars Since");
    display.setCursor(0,1);
    display.print("1st Night: ");
    display.print(totalShowCars);
}

void displayDailyTotal()  {
    display.clearDisplay();
    display.setCursor(0,line3); //Start at character 4 on line 0
    display.print("Total Cars");
    display.setCursor(0,1);
    display.print("Today: ");
    display.print(totalDailyCarstal);
}
  
void displayDate()  {
    DateTime now = rtc.now();
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("The Current Date");
    display.setCursor(0,1);
    display.print("   ");
    display.print(now.month());
    display.print("/");
    display.print(now.day());
    display.print("/");
    display.print(now.year());
}

void displayTime()  {
  DateTime now = rtc.now();
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("The Current Time");
  display.setCursor(0,1);
  display.print("    ");
  if (now.hour() <= 12)  {  
    display.print(now.hour(), DEC);
  }
  else  {
    display.print(now.hour()-12, DEC);  
  }
  display.print(':');
  if (now.minute() < 10) {
    display.print("0");  
    display.print(now.minute(), DEC);
  }
  else  {
     display.print(now.minute(), DEC);
  }
  if (now.hour() >= 12) {
    display.print(" PM");
  }
  else {
    display.print(" AM");  
  }
}

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

void beamCarDetect() // If a car is detected by a beam break, then increment the counter by 1 and add an entry to the Master.csv log file on the SD card
{

//    digitalWrite (countSuccessPin, HIGH);
    detectorTrippedCount++;                // add 1 to the counter, this prevents the code from being run more than once after tripped for 3 seconds.
    Serial.print("Cars Today:  ");
    Serial.println(totalDailyCars);

//    displayGrandTotal();
    digitalWrite(redArchPin, LOW);
    digitalWrite(greenArchPin, HIGH);
    noCarTimer = millis();

/*------Append to log file*/
    DateTime now = rtc.now();
    char buf2[] = "YYYY-MM-DD hh:mm:ss";
    Serial.print(now.toString(buf2));
    Serial.print(", Temp = ");
    Serial.print(tempF);
    Serial.print(", ");
    totalDailyCars ++;   
    totalShowCars ++;  
    Serial.print(totalDailyCars) ;  
    // open file for writing Car Data
    myFile = SD.open(fileName5, FILE_APPEND);
    if (myFile) {
        myFile.print(now.toString(buf2));
        myFile.print(", ");
        myFile.print (millis()-carDetectedMillis) ; 
        myFile.print(", 1 , "); 
        myFile.print (totalDailyCars) ; 
        myFile.print(", ");
        myFile.println(tempF);
        myFile.close();
        Serial.println(F(" = CarLog Recorded SD Card."));

          mqtt_client.publish(MQTT_PUB_TOPIC1, String(tempF).c_str());
          mqtt_client.publish(MQTT_PUB_TOPIC2, now.toString(buf2));
          mqtt_client.publish(MQTT_PUB_TOPIC3, String(totalDailyCars).c_str());
    } else {
        Serial.print(F("SD Card: Issue encountered while attempting to open the file CarCount.csv"));
    }


  updateDailyTotal(); //update total daily count in event of power failure
  updateShowTotal(); // update show total count in event of power failure

}

void setup() {
  Serial.begin(115200);
  //Initialize Display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, line1);
  display.display();

  //Initialize SD Card
  if (!SD.begin(PIN_SPI_CS)) {
    Serial.println(F("SD CARD FAILED, OR NOT PRESENT!"));

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,line1);
    display.println("Check SD Card");
    display.display();
    while (1); // stop the program and check SD Card
  }

  Serial.println(F("SD CARD INITIALIZED."));
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,line1);
    display.println("SD Card Ready");
    display.display();

  //***** Check/Prep Files for use ******/ 
  if (!SD.exists(fileName5)) {
    Serial.println(F("CarLog.csv doesn't exist. Creating CarLog.csv file..."));
    // create a new file by opening a new file and immediately close it
    myFile = SD.open("/CarLog.csv", FILE_WRITE);
    myFile.close();
      // recheck if file is created or not & write Header
    if (SD.exists("/CarLog.csv")){
      Serial.println(F("CarLog.csv exists on SD Card."));
      myFile = SD.open("/CarLog.csv", FILE_APPEND);
      myFile.println("Date Time,Millis,Car,TotalDailyCars,Temp");
      myFile.close();
      Serial.println(F("Header Written to CarLog.csv"));
    }else{
      Serial.println(F("CarLog.csv doesn't exist on SD Card."));
    }
  }

    if (!SD.exists(fileName6)) {
      Serial.println(F("DailySummary.csv doesn't exist. Creating file..."));
      // create a new file by opening a new file and immediately close it
      myFile2 = SD.open(fileName6, FILE_WRITE);
      myFile2.close();
          // recheck if file is created or not & write Header
    if (SD.exists(fileName6))  {
      Serial.println(F("DailySummary.csv exists on SD Card."));
      myFile = SD.open(fileName6, FILE_APPEND);
      myFile.println("Date, Temp, Hour1, Hour2, Hour3, Hour4, Total");
      myFile.close();
      Serial.println(F("Header Written to file DailySummary.csv"));
    }else{
      Serial.println(F("DailyTot.csv doesn't exist on SD Card."));
    }
  }
     if (!SD.exists(fileName3)) {
      Serial.println(F("CalDay.txt doesn't exist. Creating file..."));
      // create a new file by opening a new file and immediately close it
      myFile2 = SD.open(fileName3, FILE_WRITE);
      myFile2.close();
          // recheck if file is created or not & write Header
   }

    if (!SD.exists(fileName4)) {
      Serial.println(F("RunDays.txt doesn't exist. Creating file..."));
      // create a new file by opening a new file and immediately close it
      myFile2 = SD.open(fileName4, FILE_WRITE);
      myFile2.close();
          // recheck if file is created or not & write Header
   }


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
  else {
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
  
  /* Set Input Pins Detector Pins set to pulldown 10/22/24 GAL undefined before
  Detector Pins LOW when no car is present and go HIGH when beam is broken */

  firstDetectorState = 0;
  secondDetectorState = 0;
  digitalWrite(redArchPin, HIGH);
  digitalWrite(greenArchPin, HIGH);
  pinMode(firstDetectorPin, INPUT_PULLDOWN);
  pinMode(secondDetectorPin, INPUT_PULLDOWN);
  pinMode(redArchPin, OUTPUT);
  pinMode(greenArchPin, OUTPUT);
 
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, line3);
  display.println(" Ready To");
  display.println("Count Cars");

  Serial.println  ("Initializing Car Counter");
    Serial.print("Temperature: ");
    tempF=((rtc.getTemperature()*9/5)+32);
    Serial.print(tempF);
    Serial.println(" F");
  display.display();

getInitialDailyTotal();
getInitialDayRunning();
getLastDayRunning();
getShowTotal();

// Read Digital Pin States for debugging
firstDetectorState = digitalRead (firstDetectorPin); //Read the current state of the FIRST IR beam receiver/detector
secondDetectorState = digitalRead (secondDetectorPin);
digitalWrite(redArchPin, HIGH);
digitalWrite(greenArchPin, HIGH);
Serial.print("firstdetector State = ");
Serial.print(firstDetectorState);
Serial.print(" secondDetectorState = ");
Serial.println(secondDetectorState);

  delay(3000);
} //***** END SETUP ******/

void loop() {
    // non-blocking WiFi and MQTT Connectivity Checks
    if (wifiMulti.run() == WL_CONNECTED) {
      // Check for MQTT connection only if wifi is connected
      if (!mqtt_client.connected()){
        nowmqtt=millis();
        if(nowmqtt - mqtt_lastReconnectAttemptMillis > mqtt_connectionCheckMillis){
          mqtt_lastReconnectAttemptMillis = nowmqtt;
          Serial.println("Attempting MQTT Connection");
          reconnect();
        }
          mqtt_lastReconnectAttemptMillis =0;
      } else {
        //keep MQTT client connected when WiFi is connected
        mqtt_client.loop();
      }
    } else {
        // Reconnect WiFi if lost, non blocking
        nowwifi=millis();
          if ((nowwifi - wifi_lastReconnectAttemptMillis) > wifi_connectioncheckMillis){
            setup_wifi();
          }
        wifi_lastReconnectAttemptMillis = 0;
    }
     

      DateTime now = rtc.now();
      tempF=((rtc.getTemperature()*9/5)+32);
      //Reset Car Counter at 5:00:00 pm
        if ((now.hour() == 16) && (now.minute() == 55) && (now.second() == 0)){
             totalDailyCars = 0;
         }
       //Write Totals at 9:10:00 pm 
        if ((now.hour() == 21) && (now.minute() == 10) && (now.second() == 0)){
             WriteTotals();
        }
      
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, line1);
      //  display Day of Week
      display.print(days[now.dayOfTheWeek()]);

      //  Display Date
      display.print(" ");         
      display.print(months[now.month(), DEC +1]);
      display.print(" ");
      display.print(now.day(), DEC);
      display.print(", ");
      display.println(now.year(), DEC);

      // Convert 24 hour clock to 12 hours
      currentHour = now.hour();

      if (currentHour - 12 > 0) {
          ampm ="PM";
          currentHour = now.hour() - 12;
      }else{
          currentHour = now.hour();
          ampm = "AM";
      }

      //Display Time
      //add leading 0 to Hours & display Hours
      display.setTextSize(1);
      if (currentHour < 10){
        display.setCursor(0, line2);
        display.print("0");
        display.println(currentHour, DEC);
      }else{
        display.setCursor(0, line2);
        display.println(currentHour, DEC);
      }
      display.setCursor(14, line2);
      display.println(":");
      if (now.minute() < 10) {
        display.setCursor(20, line2);
        display.print("0");
        display.println(now.minute(), DEC);
      }else{
        display.setCursor(21, line2);
        display.println(now.minute(), DEC);
      }
      display.setCursor(34, line2);
      display.println(":");
      if (now.second() < 10){
        display.setCursor(41, line2);
        display.print("0");
        display.println(now.second(), DEC);
      }else{
        display.setCursor(41, line2);
        display.println(now.second(), DEC);   
      }

      // Display AM-PM
      display.setCursor(56, line2);
      display.println(ampm); 

      // Display Temp
      display.setCursor(73, line2);
      display.print("Temp: " );
      display.println(tempF, 0);

      // Display Car Count
      display.setTextSize(2);
      display.setCursor(0, line4);
      display.print("Cars: ");
      display.println(totalDailyCars);

      display.display();

      //Save Hourly Totals
      if (now.minute()==0 && now.second()==0){
        HourlyTotals();
      }

/* LOOP PIN STATE FOR DEBUG 
digitalWrite(redArchPin, HIGH);
digitalWrite(greenArchPin, HIGH);
Serial.print("firstdetector State = ");
Serial.print(firstDetectorState);
Serial.print(" secondDetectorState = ");
Serial.println(secondDetectorState);
*/

//***** DETECT CARS *****/
  firstDetectorState = digitalRead (firstDetectorPin); //Read the current state of the FIRST IR beam receiver/detector
  secondDetectorState = digitalRead (secondDetectorPin); //Read the current state of the SECOND IR beam receiver/detector
  
  // Bounce check, if the beam is broken start the timer and turn on only the green arch
  if (secondDetectorState == LOW && previousSecondDetectorState == HIGH && millis()- detectorMillis > 200) 
    {
    digitalWrite(redArchPin, LOW); // Turn Red Arch Off
    digitalWrite(greenArchPin, HIGH); // Turn Green Arch On
    detectorMillis = millis();    // if the beam is broken remember the time 
    /* LOOP PIN STATE FOR DEBUG */
    Serial.print("Bounce Check ... ");
    digitalWrite(redArchPin, HIGH);
    digitalWrite(greenArchPin, LOW);
    Serial.print("firstdetector State = ");
    Serial.print(firstDetectorState);
    Serial.print(" secondDetectorState = ");
    Serial.println(secondDetectorState);

    }
  
  // If the SECOND beam has been broken for more than 0.50 second (1000= 1 Second) & the FIRST beam is broken
  if (secondDetectorState == LOW && ((millis() - detectorMillis) % 500) < 20 && millis() - detectorMillis > 500 && detectorTrippedCount == 0 && firstDetectorState == LOW) 
    {
      //***** CAR PASSED *****/
      /*Call the subroutine that increments the car counter and appends the log with an entry for the 
      vehicle when the IR beam is broken */
 
      /* CAR DETECTED DEBUG */
      Serial.print("Call to BEAM CAR DETECT ... ");
      digitalWrite(redArchPin, LOW);
      digitalWrite(greenArchPin, HIGH);
      Serial.print("firstdetector State = ");
      Serial.print(firstDetectorState);
      Serial.print(" secondDetectorState = ");
      Serial.println(secondDetectorState);

      beamCarDetect();  
    }
    
  /*--------- Reset the counter if the beam is not broken --------- */    
 if (secondDetectorState == HIGH)  //Check to see if the beam is not broken, this prevents the green arch from never turning off)
   {
      if (detectorTrippedCount != 0)
      digitalWrite(redArchPin, LOW);// Turn Red Arch Off

      /* CAR DETECTED DEBUG */
      Serial.print("Waiting to start Idle Pattern... ");
      digitalWrite(redArchPin, LOW);
      digitalWrite(greenArchPin, HIGH);
      Serial.print("firstdetector State = ");
      Serial.print(firstDetectorState);
      Serial.print(" secondDetectorState = ");
      Serial.println(secondDetectorState);

      
      if(millis() - noCarTimer >= 30000) // If the beam hasn't been broken by a vehicle for over 30 seconds then start a pattern on the arches.
        {
        //***** PLAY PATTERN WHEN NO CARS PRESENT *****/
        playPattern();
        }
      else
      {
        digitalWrite(greenArchPin, HIGH); // Turn Green Arch On
      }
      detectorTrippedCount = 0;
    }
//***** END OF CAR DETECTION *****/


/*-------- Rotate through LCD Displays for Grand Total, Total Today, Current Date & Current Time - change every 5 seconds ---------*/
if (displayMode == 0 && (millis() - displayModeMillis) >= 5000)
  {
  displayGrandTotal();
  displayMode++;
  displayModeMillis = millis();
  }
if (displayMode == 1 && (millis() - displayModeMillis) >= 5000)
  {
  displayDailyTotal();
  displayMode++;
  displayModeMillis = millis();
  }
/*
if (displayMode == 2 && (millis() - displayModeMillis) >= 5000)
  {
  displayDate();
  displayMode++;
  displayModeMillis = millis();
  }      
if (displayMode == 3 && (millis() - displayModeMillis) >= 5000)
  {
  displayTime();
  displayMode = 0;
  displayModeMillis = millis();
  }    
*/

/*-------- Reset the detector and button state to 0 for the next go-around of the loop ---------*/    
previousSecondDetectorState = secondDetectorState; // Reset detector state
//Repeat Loop
}