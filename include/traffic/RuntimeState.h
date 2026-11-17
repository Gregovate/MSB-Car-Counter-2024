#pragma once
#include "Platform.h"
#include "DeviceConfig.h"
#include "MqttTopics.h"

// Transitional shared state. Defined exactly once in RuntimeState.cpp.
// Service ownership is documented in Modular_Firmware.md.
enum PatternState {
    INITIAL_OFF,
    RED_ON,
    GREEN_ON,
    BOTH_ON,
    BOTH_OFF,
    RED_OFF_GREEN_ON,
    GREEN_OFF_RED_ON
};

enum CarDetectState {
    WAITING_FOR_CAR,
    FIRST_BEAM_HIGH,
    BOTH_BEAMS_HIGH,
    CAR_DETECTED
};

extern unsigned int waitDuration;
extern const int showStartMinDefault;
extern const int showEndMinDefault;
extern unsigned long lastSdCheck;
extern int showStartMin;
extern int showEndMin;
extern bool rtcReady;
extern String bootTimestamp;
extern volatile bool pendingSaveDaily;
extern volatile bool pendingSaveShow;
extern volatile bool pendingSaveHourly;
extern unsigned long lastKeepAliveMillis;
extern const unsigned long TEMP_PUB_INTERVAL_MS;
extern unsigned long lastTempPubMillis;
extern int mqttKeepAlive;
extern char topic[60];
extern char topicBase[60];
extern PatternState currentPatternState;
extern unsigned long patternModeMillis;
extern CarDetectState currentCarDetectState;
extern bool carPresentFlag;
extern unsigned long firstBeamTripTime;
extern unsigned long bothBeamsHighTime;
extern unsigned long lastCarDetectedMillis;
extern unsigned long secondBeamTripTime;
extern unsigned long timeBetweenCars_ms;
extern bool abFollowCaptured;
extern unsigned long abFollow_ms;
extern unsigned long firstBeamHealth_ms;
extern unsigned long secondBeamHealth_ms;
extern unsigned long timeToPassMS;
extern int firstBeamState;
extern int secondBeamState;
extern int prevFirstBeamState;
extern int prevSecondBeamState;
extern unsigned long debounceDelay;
extern unsigned long minActivationDuration;
extern unsigned long carCounterTimeout;
extern bool carStuckAlarmActive;
extern unsigned long noCarTimer;
extern bool stableFirstBeamState;
extern bool stableSecondBeamState;
extern unsigned long lastFirstBeamChangeTime;
extern unsigned long lastSecondBeamChangeTime;
extern AsyncWebServer server;
extern String currentDirectory;
extern unsigned long ota_progress_millis;
extern RTC_DS3231 rtc;
extern const char* ampm;
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;
extern DHT dht;
extern float tempF;
extern float humidity;
extern int showStartY;
extern int showStartM;
extern int showStartD;
extern bool showStartDateValid;
extern Adafruit_SSD1306 display;
extern const int line1;
extern const int line2;
extern const int line3;
extern const int line4;
extern const int line5;
extern const int line6;
extern const int line7;
extern WiFiMulti wifiMulti;
extern WiFiClient espCarCounter;
extern uint16_t connectTimeOutPerAP;
extern PubSubClient mqtt_client;
extern unsigned long lastMsg;
extern char msg[MSG_BUFFER_SIZE];
extern bool mqtt_connected;
extern bool wifi_connected;
extern int wifi_connect_attempts;
extern unsigned int dayOfMonth;
extern unsigned int lastDayOfMonth;
extern unsigned int currentHr12;
extern unsigned int currentHr24;
extern unsigned int currentMin;
extern unsigned int currentSec;
extern unsigned int daysRunning;
extern unsigned int currentTimeMinute;
extern int totalDailyCars;
extern int totalShowCars;
extern bool totalsInitialized;
extern const unsigned long wifi_connectioncheckMillis;
extern const unsigned long mqtt_connectionCheckMillis;
extern unsigned long start_MqttMillis;
extern unsigned long start_WiFiMillis;
extern bool flagDaysRunningReset;
extern bool flagMidnightReset;
extern bool flagDailyShowStartReset;
extern bool flagDailySummarySaved;
extern bool flagDailyShowSummarySaved;
extern bool flagHourlyReset;
extern bool showTime;
extern bool resetFlagsOnce;
extern bool sdAvailable;
extern char seasonFolder[32];
extern File myFile;
extern const String fileName1;
extern const String fileName2;
extern const String fileName3;
extern const String fileName4;
extern const String fileName5;
extern const String fileName6;
extern const String fileName7;
extern const String fileName8;
extern const String fileName9;
extern const String fileName10;
extern unsigned int hourlyCount[24];
extern float hourlyTemp[24];
extern char days[7][4];
extern char months[12][4];
extern std::queue<String> publishQueue;
extern const size_t MAX_QUEUE;
extern const size_t MAX_FLUSH_PER_CALL;
