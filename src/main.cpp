
/*Making Spirits Bright Traffic Counter and Collection Area Light Control
Created November 2013 by Andrew Bubb - andrewmbubb@gmail.com
Version 2014.1.0 (Based off Version 6.5) (11/13/2014 8:04pm)
Removed all reference to manual counter button.  Added second beam. Temporarily commented out startup messages to speed testing.
Revised for Integration to ESP32 for real time data logging Greg Liebig 12/7/23 9:25 pm with countSuccessPin 8

Started Rewrite for ESP32 10/19/24 
*/


/*-----( Import needed libraries )-----*/
#include <Arduino.h>
#include <Wire.h>
#include <PubSubClient.h>
//#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"
#include "NTPClient.h"
//#include <WiFiClientSecure.h>
#include <WiFiMulti.h>
#include "secrets.h"
#include "time.h"
//#include "FS.h"
#include "SD.h"

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
#define firstDetectorPin 32
#define secondDetectorPin 33
//#define countSuccessPin 8
#define PIN_SPI_CS 5 // SD Card CS GPIO5
#define redArchPin 25
#define greenArchPin 26
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define FWVersion "24.10.19.1" // Firmware Version
#define OTA_Title "Car Counter" // OTA Title
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


//rtc_DS1307 RTC;
RTC_DS3231 rtc;

int line1 =0;
int line2 =9;
int line3 = 20;
int line4 = 30;
int line5 = 42;
int line6 = 50;
int line7 = 53;

WiFiMulti wifiMulti;
//WiFiClientSecure espGateCounter;
WiFiClient espGateCounter;
PubSubClient mqtt_client(espGateCounter);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
int value = 0;

char mqtt_server[] = mqtt_Server;
char mqtt_username[] = mqtt_UserName;
char mqtt_password[] = mqtt_Password;
const int mqtt_port = mqtt_Port;

// MQTT TOPIC DEFINITIONS
#define THIS_MQTT_CLIENT "espCarCounter" // Look at line 90 and set variable for WiFi Client secure & PubSubCLient 12/23/23

// Puplishing Topics 
#define MQTT_PUB_TOPIC0  "msb/traffic/enter/hello"
#define MQTT_PUB_TOPIC1  "msb/traffic/enter/temp"
#define MQTT_PUB_TOPIC2  "msb/traffic/enter/time"
#define MQTT_PUB_TOPIC3  "msb/traffic/enter/count"
#define MQTT_PUB_TOPIC4  "msb/traffic/enter/inpark"
#define MQTT_PUB_TOPIC5  "msb/traffic/enter/debug/timeout"
#define MQTT_PUB_TOPIC6  "msb/traffic/enter/debug/beamSensorState"
#define MQTT_PUB_TOPIC7  "msb/traffic/enter/temp"
// Subscribing Topics (to reset values)
#define MQTT_SUB_TOPIC0  "msb/traffic/enter/count"
#define MQTT_SUB_TOPIC1  "msb/traffic/exit/resetcount"

//const uint32_t connectTimeoutMs = 10000;
uint16_t connectTimeOutPerAP=5000;
const char* ampm ="AM";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;
const int   daylightOffset_sec = 3600;
int16_t temp;

char buf2[25] = "YYYY-MM-DD hh:mm:ss";

// ******** RESET COUNTS ON REBOOT *******
/*
// ******** RESET COUNTS ON REBOOT *******
int currentDay = 0;
int currentHour = 0;
int currentMin = 0;
int totalDailyCars = 0;
int carCounterCars =0;
int sensorBounceCount=0;
int sensorBounceRemainder;
bool sensorBounceFlag;
*/



/*-----( Declare Constants )-----*/


/*SD Card Files
DAILYTOT.TXT
MASTER.CSV
NOW.TXT
RUNNING.TXT
TOTAL.TXT
readme.txt*/



// **********FILE NAMES FOR SD CARD *********
File Total;  // file to store total counts
File Daily; //
File ShowLog; // was MASTER.CSV
File Now; //
File Running; //

// **********RTC HUMAN READABLE NAMES *********
char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/*-----( Declare Variables )-----*/
int firstDetectorState;  // Holds the current state of the FIRST IR receiver/detector
int secondDetectorState;  // Holds the current state of the SECOND IR receiver/detector

int previousFirstDetectorState; // Holds the previous state of the FIRST IR receiver/detector
int previousSecondDetectorState; // Holds the previous state of the SECOND IR receiver/detector


unsigned long detectorMillis = 0;
// unsigned long buttonMillis = 0;
int detectorTrippedCount = 0;
unsigned long noCarTimer = 0;
unsigned int totalCars;
unsigned int dailyTotal;  
int displayMode = 0;
unsigned long displayModeMillis = 0;
unsigned long dayMillis = 0;
int currentDay;
int daysRunning;
int alternateColorMode = 0;
int patternMode = 0;
unsigned long patternModeMillis = 0;

void setup_wifi() {
    Serial.println("Connecting to WiFi");
    display.println("Connecting to WiFi..");
    display.display();
    while(wifiMulti.run(connectTimeOutPerAP) != WL_CONNECTED) {
      //Serial.print(".");
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



void getInitialTotal()   // open TOTAL.txt to get initial totalCars value
  {
  ifstream sdin("TOTAL.TXT");
  // check for an open failure
  if (!sdin) sd.errorHalt("sdin failed");
  sdin >> totalCars;
  sdin.close();
  }

void getInitialDailyTotal()   // open DAILYTOT.txt to get initial dailyTotal value
  {
  ifstream sdin("DAILYTOT.TXT");
  // check for an open failure
  if (!sdin) sd.errorHalt("sdin failed");
  sdin >> dailyTotal;
  sdin.close();
  }

void getInitialDaysRunning()   // open TOTAL.txt to get initial number of days the system has been running value
  {
  ifstream sdin("RUNNING.TXT");
  // check for an open failure
  if (!sdin) sd.errorHalt("sdin failed");
  sdin >> daysRunning;
  sdin.close();
  }
  


void incrementTotalFile()
/*------- Increment the grand total cars file ------------ */
{
   myFile = SD.open("/TOTAL.TXT");
  if (myFile) {
  myFile.print(totalCars++);
  myFile.close();
    } else {
    Serial.print(F("SD Card: Cannot open the file TOTAL.TXT"));
  }

}

void incrementDailyTotal()
  {
  myFile = SD.open("/DAILYTOT.TXT");
  if (myFile) {
    myFile.print(dailyTotal++);
  } else {
    Serial.print(F("SD Card: Cannot open the file DAILYTOT.TXT"));
  }
  
  }

void displayGrandTotal()
  {
    display.clearDisplay();
    display.setCursor(0,0); //Start at character 4 on line 0
    display.print("Total Cars Since");
    display.setCursor(0,1);
    display.print("1st Night: ");
    display.print(totalCars);
  }

void displayDailyTotal()
  {
    display.clearDisplay();
    display.setCursor(0,0); //Start at character 4 on line 0
    display.print("Total Cars");
    display.setCursor(0,1);
    display.print("Today: ");
    display.print(dailyTotal);
  }
  
/* void displayDailyAverage()
  {
    display.clearDisplay();
    display.setCursor(0,0); //Start at character 1 on line 0
    display.print("Daily Average");
    display.setCursor(0,1);
    display.print(totalCars / daysRunning);
  }

void displayHourlyAverage()
  {
    display.clearDisplay();
    display.setCursor(0,0); //Start at character 1 on line 0
    display.print("Hourly Average");
    display.setCursor(0,1);
    display.print(totalCars / (daysRunning * 4));
  }
Averages no longer displayed.  Removing subroutines from code in next version (6.4) */

void displayDate()
  {
    DateTime now = rtc.now();
    display.clear();
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
  
void displayTime()
  {
    DateTime now = rtc.now();
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("The Current Time");
  display.setCursor(0,1);
  display.print("    ");
  if (now.hour() <= 12)
    {  
    display.print(now.hour(), DEC);
    }
  else
    {
    display.print(now.hour()-12, DEC);  
    }
  display.print(':');
  if (now.minute() < 10)
    {
    display.print("0");  
    display.print(now.minute(), DEC);
    }
  else
    {
     display.print(now.minute(), DEC);
    }
  if (now.hour() >= 12)
    {
    display.print(" PM");
    }
  else
    {
    display.print(" AM");  
    }
  }
  
 void resetDailyTotal()
  {
  
    if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
   
    // open the output file
    ofstream sdout("DAILYTOT.TXT");
    // Reset the total vehicle count
    sdout << dailyTotal << endl;
    
    if (!sdout) sd.errorHalt("sdout failed");
  
    sdout.close();
    
  }

void incrementDaysRunning()
    {
  myFile = SD.open("/RUNNING.TXT");
  if (myFile) {
  // check for an open failure
  myFile.print(daysRunning++);
  myFile.close();
  }else{
    
  // open the output file
  ofstream sdout("RUNNING.TXT");
  // Replace the number of days the program has been running
  sdout << daysRunning << endl;
  
  if (!sdout) sd.errorHalt("sdout failed");

  sdout.close();
  }

void checkDay() // See if it's a new day and if so, reset int dailyTotal
  {
    if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();
    // open the input file
    ifstream sdin("NOW.TXT");
    // check for an open failure
    if (!sdin) sd.errorHalt("sdin failed");
    sdin >> currentDay; //---------------------------------------------------------------------------------------------------------!!!!!!!!!!!!!!!!!!!!!!!!!!!!!111
    sdin.close();
    DateTime now = rtc.now();
    now = rtc.now();
    if (currentDay != now.day())
    {
      Serial.println("New Day");
      currentDay = now.day();
      // open the output file
      ofstream sdout("NOW.TXT");
      // Replace existing with current date
      sdout << currentDay << endl;
      
      if (!sdout) sd.errorHalt("sdout failed");
    
      sdout.close();
      dailyTotal=0;
      resetDailyTotal();
      incrementDaysRunning();
    }
  }  





void beamCarDetect () // If a car is detected by a beam break, then increment the counter by 1 and add an entry to the Master.csv log file on the SD card
{
    DateTime now = rtc.now();
//    digitalWrite (countSuccessPin, HIGH);
    detectorTrippedCount++;                // add 1 to the counter, this prevents the code from being run more than once after tripped for 3 seconds.
    Serial.print("Cars Today:  ");
    Serial.println(dailyTotal);
/*
    display.noBacklight();
    delay(125);
    display.backlight();
    delay(125);    
*/

//    displayGrandTotal();
    digitalWrite(redArchPin, LOW);
    digitalWrite(greenArchPin, HIGH);
    noCarTimer = millis();

/*------Append to log file*/

  now = rtc.now();
  // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
  // breadboards.  use SPI_FULL_SPEED for better performance.
  if (!sd.begin(chipSelect, SPI_HALF_SPEED)) sd.initErrorHalt();


//    cout << pstr("Appending to: ") << "MASTER.CSV" << endl;
  
    // open stream for append
    ofstream sdout("MASTER.CSV", ios::out | ios::app);
    if (!sdout) error("open failed");

    int currentMonth = now.month();
    int currentDay = now.day();
    int currentYear = now.year();
    int currentHour = now.hour();
    int currentMinute = now.minute();
    int currentSecond = now.second();

    sdout << currentMonth << "/" << currentDay << "/" << currentYear << "," << currentHour << ":" << currentMinute << ":" << currentSecond <<endl; // Append MM/DD/YYYY,HH:MM:SS event to Master log on SD
//    cout << currentMonth << "/" << currentDay << "/" << currentYear << "," << currentHour << ":" << currentMinute << ":" << currentSecond <<endl; // Display MM/DD/YYYY,HH:MM:SS event to Serial
    // close the stream
    sdout.close();

    if (!sdout) error("append data failed");
//    cout << endl << "Done" << endl;

  incrementTotalFile();
  incrementDailyTotal();
//  digitalWrite (countSuccessPin, LOW);
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

void setup()   /*----( SETUP: RUNS ONCE )----*/
{
  // initialize serial communication:
  Serial.begin(115200);
  //Initialize Display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.display();  

  //Initialize SD Card
  if (!SD.begin(PIN_SPI_CS)) {
    Serial.println(F("SD CARD FAILED, OR NOT PRESENT!"));
    while (1); // stop the program
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,line1);
    display.println("Check SD Card");
    display.display();
  }

  if (!SD.exists("/GateCount.csv")) {
    Serial.println(F("GateCount.csv doesn't exist. Creating GateCount.csv file..."));
    // create a new file by opening a new file and immediately close it
    myFile = SD.open("/GateCount.csv", FILE_WRITE);
    myFile.close();
      // recheck if file is created or not & write Header
    if (SD.exists("/GateCount.csv")){
      Serial.println(F("GateCount.csv exists on SD Card."));
      myFile = SD.open("/GateCount.csv", FILE_APPEND);
      myFile.println("Date Time,Pass Timer,NoCar Timer,Bounces,Car#,Cars In Park,Temp,Last Car Millis, This Car Millis,Bounce Flag,Millis");
      myFile.close();
      Serial.println(F("Header Written to GateCount.csv"));
    }else{
      Serial.println(F("GateCount.csv doesn't exist on SD Card."));
    }
  }


  if (!SD.exists("/SensorBounces.csv")) {
    Serial.println(F("SensorBounces.csv doesn't exist. Creating SensorBounces.csv file..."));
    // create a new file by opening a new file and immediately close it
    myFile2 = SD.open("/SensorBounces.csv", FILE_WRITE);
    myFile2.close();
   // recheck if file is created or not & write Header
    if (SD.exists("/SensorBounces.csv")){
      Serial.println(F("SensorBounces.csv exists on SD Card."));
      myFile2 = SD.open("/SensorBounces.csv", FILE_APPEND);
      //("DateTime\t\t\tPassing Time\tLast High\tDiff\tLow Millis\tLast Low\tDiff\tBounce #\tCurent State\tCar#" )
      myFile2.println("DateTime,TimeToPass,Last Beam Low,Beam Low,Diff,Beam State, Bounce#,Car#");
      myFile2.close();
      Serial.println(F("Header Written to SensorBounces.csv"));
    }else{
      Serial.println(F("SensorBounces.csv doesn't exist on SD Card."));
    }
 
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
  } else {
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
//  espGateCounter.setCACert(root_ca);
  mqtt_client.setServer(mqtt_server, mqtt_port);
  mqtt_client.setCallback(callback);

  //If RTC not present, stop and check battery
  if (! rtc.begin()) {
    Serial.println("Could not find RTC! Check circuit.");
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,line1);
    display.println("Clock DEAD");
    display.display();
    while (1);
  }

  // Get NTP time from Time Server 
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  SetLocalTime();

 //Set I/O Pins
  firstDetectorState = 0;
  secondDetectorState = 0;
  digitalWrite(redArchPin, HIGH);
  digitalWrite(greenArchPin, HIGH);
  pinMode(firstDetectorPin, INPUT);
  pinMode(secondDetectorPin, INPUT);
  pinMode(redArchPin, OUTPUT);
  pinMode(greenArchPin, OUTPUT);
//  pinMode(countSuccessPin, OUTPUT);
//  digitalWrite(countSuccessPin, LOW);

  

  
 /*
  // ------- Quick 3 blinks of display backlight  -------------
  for(int i = 0; i< 3; i++)
  {
    display.backlight();
    delay(250);
    display.noBacklight();
    delay(250);
  }
  display.backlight(); // finish with display backlight on  
  */


//-------- Write the startup messages on the display display ------------------
// NOTE: Cursor Position: (CHAR, LINE) start at 0,0 for leftmost line 1 start at 0,1 for leftmost line 2  
  
/*  display.setCursor(0,0);
  display.print(" Making Spirits");
  display.setCursor(0,1);
  display.print("     Bright");
  delay(4000);
  display.clear();  
  display.setCursor(0,0);
  display.print("Traffic Counter");
  display.setCursor(0,1);
  display.print("& Light Control");
  delay(4000);  
  display.clear();
  display.setCursor(0,0);
  display.print(" WARNING - HIGH");
  display.setCursor(0,1);
  display.print(" VOLTAGE INSIDE!");
  delay(4000);  
  display.clear();
  display.setCursor(0,0);
  display.print("DISCONNECT POWER");
  display.setCursor(0,1);
  display.print("BEFORE OPENING!!");
  delay(4000);  
  display.clear();
  display.setCursor(0,0);
  display.print("Instructions On");
  display.setCursor(0,1);
  display.print("SD Card in Unit");
  delay(4000);  
  displayDate(); //Display the current Date on the display display
  delay(4000);
  displayTime(); //Display the current Time on the display display
  delay(4000); 
  
//  Removed startup information display section to speed up testing, found it was not needed for normal operation and will leave commented out */ 
  
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, line4);
  display.print("Ready to ");

  Serial.println  ("Count Cars");
    Serial.print("Temperature: ");
    temp=((rtc.getTemperature()*9/5)+32);
    Serial.print(temp);
    Serial.println(" F");
  display.display();
  digitalWrite(greenArchPin, LOW); // Turn Green Arch On
  
  ElegantOTA.setAutoReboot(true); // allow reboot after OTA Upload

    if(!MDNS.begin("esp32CarCounter")) {
     Serial.println("Error starting mDNS");
     return;
  }
  delay(3000);  


  

getInitialTotal();
getInitialDailyTotal();
getInitialDaysRunning();
checkDay();
displayModeMillis = millis();
noCarTimer = millis();

}/*--(end setup)--*/




void  loop ()  /*----( LOOP: RUNS CONSTANTLY )----*/
{
  //reset daily total at 4:55 pm when park opens to eliminate traffic that happens while car counter is on and there is non-show traffic 
    DateTime now = rtc.now();
    now = rtc.now();

  if ((now.hour() == 16) && (now.minute() == 55) && (now.second() == 0)){
     dailyTotal = 0;
   }
  firstDetectorState = digitalRead (firstDetectorPin); //Read the current state of the FIRST IR beam receiver/detector
  secondDetectorState = digitalRead (secondDetectorPin); //Read the current state of the SECOND IR beam receiver/detector

  if (secondDetectorState == LOW && previousSecondDetectorState == HIGH && millis()- detectorMillis > 200) // Bounce check, if the beam is broken start the timer and turn on only the green arch
    {
    digitalWrite(redArchPin, HIGH); // Turn Red Arch Off
    digitalWrite(greenArchPin, LOW); // Turn Green Arch On
    detectorMillis = millis();    // if the beam is broken remember the time 
    }

  if (secondDetectorState == LOW && ((millis() - detectorMillis) % 500) < 20 && millis() - detectorMillis > 500 && detectorTrippedCount == 0 && firstDetectorState == LOW) // If the SECOND beam has been broken for more than 0.50 second (1000= 1 Second) & the FIRST beam is broken
    {
      beamCarDetect();  //Call the subroutine that increments the car counter and appends the log with an entry for the vehicle when the IR beam is broken
    }
    
/*--------- Reset the counter if the beam is not broken --------- */    
 if (secondDetectorState == HIGH)  //Check to see if the beam is not broken, this prevents the green arch from never turning off)
   {
      if (detectorTrippedCount != 0)
      digitalWrite(redArchPin, HIGH);// Turn Red Arch Off
      if(millis() - noCarTimer >= 30000) // If the beam hasn't been broken by a vehicle for over 30 seconds then start a pattern on the arches.
        {
        playPattern();
        }
      else
      {
        digitalWrite(greenArchPin, LOW); // Turn Green Arch On
      }
      detectorTrippedCount = 0;
   
   
   }

/*-------- Rotate through display Displays for Grand Total, Total Today, Current Date & Current Time - change every 5 seconds ---------*/
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


/*-------- Reset the detector and button state to 0 for the next go-around of the loop ---------*/    
previousSecondDetectorState = secondDetectorState; // Reset detector state

}/* --(end main loop )-- */

/* ( THE END ) */