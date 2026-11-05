#include "traffic/RuntimeState.h"

// Single instances retain the original initialization values and ordering.
unsigned int waitDuration = 950; // minimum millis for secondBeam to be broken needed to detect a car
const int showStartMinDefault = 17*60; // default 5:00 pm
const int showEndMinDefault = 21*60 + 10; // default 9:10 pm
unsigned long lastSdCheck = 0; // GAL 25-11-23: last SD health check time
int showStartMin = showStartMinDefault; // live value (HA can override)
int showEndMin = showEndMinDefault; // live value (HA can override later)
bool rtcReady = false; // GAL 25-11-22: RTC boot-safety guard
String bootTimestamp = ""; // GAL 25-11-22: Store boot timestamp for logging
volatile bool pendingSaveDaily = false; // GAL 25-11-23.x: Flag to defer daily save to safe context
volatile bool pendingSaveShow = false; // GAL 25-11-23.x: Flag to defer show summary save to safe context
volatile bool pendingSaveHourly = false; // GAL 25-11-24: Flag to defer hourly save to safe context
unsigned long lastKeepAliveMillis = 0;
const unsigned long TEMP_PUB_INTERVAL_MS = 10UL * 60UL * 1000UL; // 10 minutes
unsigned long lastTempPubMillis = 0;
int mqttKeepAlive = 30; // GAL 25-11-22.4: heartbeat interval (seconds)
char topic[60]; // keep for dynamic hourly topic building
char topicBase[60]; // keep until confirmed unused
PatternState currentPatternState = INITIAL_OFF;
unsigned long patternModeMillis = 0;
CarDetectState currentCarDetectState = WAITING_FOR_CAR;
bool carPresentFlag = false; // Flag to ensure a car is only counted once
unsigned long firstBeamTripTime = 0; // ms when first beam was tripped
unsigned long bothBeamsHighTime = 0; // ms when both beams were tripped
unsigned long lastCarDetectedMillis = 0; // ms when last car was detected
unsigned long secondBeamTripTime = 0; // ms when second beam was tripped
unsigned long timeBetweenCars_ms = 0; // Time between last car and current car
bool abFollowCaptured = false;
unsigned long abFollow_ms = 0; // Time in ms between first beam and second beam trip
unsigned long firstBeamHealth_ms = 0;
unsigned long secondBeamHealth_ms = 0;
unsigned long timeToPassMS = 0;
int firstBeamState; // Holds the current state of the FIRST IR receiver/Beam
int secondBeamState; // Holds the current state of the SECOND IR receiver/Beam
int prevFirstBeamState = -1; // Holds the previous state of the FIRST IR receiver/Beam
int prevSecondBeamState = -1; // Holds the previous state of the SECOND IR receiver/Beam
unsigned long debounceDelay = 50; // ms – IR input debounce
unsigned long minActivationDuration = 100; // ms – minimum duration for valid activation
unsigned long carCounterTimeout = 60000; // ms
bool carStuckAlarmActive = false;
unsigned long noCarTimer = 0; // ms
bool stableFirstBeamState = LOW;
bool stableSecondBeamState = LOW;
unsigned long lastFirstBeamChangeTime = 0;
unsigned long lastSecondBeamChangeTime = 0;
AsyncWebServer server(80); // Define Webserver
String currentDirectory = "/"; // Current working directory
unsigned long ota_progress_millis = 0;
RTC_DS3231 rtc;
const char* ampm ="AM";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600;
const int daylightOffset_sec = 3600;
DHT dht(DHTPIN, DHTTYPE);
float tempF = -998.0; // Temperature
float humidity = -998.0; // Humidity
int showStartY = 0; // Show Start Year
int showStartM = 0; // Show Start Month
int showStartD = 0; // Show Start Day
bool showStartDateValid = false;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const int line1 =0;
const int line2 =9;
const int line3 = 19;
const int line4 = 30;
const int line5 = 42;
const int line6 = 50;
const int line7 = 53;
WiFiMulti wifiMulti;
WiFiClient espCarCounter;
uint16_t connectTimeOutPerAP=5000;
PubSubClient mqtt_client(espCarCounter);
unsigned long lastMsg = 0;
char msg[MSG_BUFFER_SIZE];
bool mqtt_connected = false;
bool wifi_connected = false;
int wifi_connect_attempts = 5;
unsigned int dayOfMonth; // Current Calendar day
unsigned int lastDayOfMonth; // Last calendar day used to reset days running
unsigned int currentHr12; // Current Hour 12 Hour format
unsigned int currentHr24; // Current Hour 24 Hour Format
unsigned int currentMin; // Current Minute
unsigned int currentSec; // Current Second
unsigned int daysRunning; // Number of days the show is running.
unsigned int currentTimeMinute; // for converting clock time hh:mm to clock time min since midnight
int totalDailyCars; // total cars counted per day 24/7 Needed for debugging
int totalShowCars; // total cars counted for durning show hours open (5:00 pm to 9:10 pm)
bool totalsInitialized = false; // GAL 25-11-29: true once loaded from SD or MQTT
const unsigned long wifi_connectioncheckMillis = 5000; // check for connection every 5 sec
const unsigned long mqtt_connectionCheckMillis = 30000; // check for connection
unsigned long start_MqttMillis; // for Keep Alive Timer
unsigned long start_WiFiMillis; // for keep Alive Timer
bool flagDaysRunningReset = false;
bool flagMidnightReset = false;
bool flagDailyShowStartReset = false;
bool flagDailySummarySaved = false;
bool flagDailyShowSummarySaved = false;
bool flagHourlyReset = false;
bool showTime = false;
bool resetFlagsOnce = false;
bool sdAvailable = false;
char seasonFolder[32] = "/CC/0000"; // default, updated at boot
File myFile; // used to write files to SD Card
const String fileName1 = "/EnterTotal.txt"; // DailyTot.txt file to store daily counts in the event of a Failure
const String fileName2 = "/ShowTotal.txt"; // ShowTot.txt file to store season total counts
const String fileName3 = "/DayOfMonth.txt"; // DayOfMonth.txt file to store current day number
const String fileName4 = "/RunDays.txt"; // RunDays.txt file to store days since open
const String fileName5 = "/HourlyData.csv"; // HourlyData.csv Stores Daily Totals by Hour and total
const String fileName6 = "/EnterLog.csv"; // EnterLog.csv file to store all car counts for season (was MASTER.CSV)
const String fileName7 = "/ShowSummary.csv"; // Show summary of counts during show (5:00pm to 9:10pm)
const String fileName8 = "/data/index.html"; // data folder and index.html for serving files OTA
const String fileName9 = "/data/style.css"; // data folder and index.html for serving files OTA
const String fileName10 = "/ShowSummaryBuckets.csv"; // New incremental summary version
unsigned int hourlyCount[24] = {0}; // Array for Daily total cars per hour
float hourlyTemp[24] = {0.0}; // Array to store average temperatures for 24 hours
char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
std::queue<String> publishQueue;
const size_t MAX_QUEUE = 3000;
const size_t MAX_FLUSH_PER_CALL = 150; // prevents WDT resets
