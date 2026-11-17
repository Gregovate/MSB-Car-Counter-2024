#include "traffic/Firmware.h"
#include "traffic/PassagePolicy.h"

// Extracted from the production baseline; behavior fixes are separate changes.
void countTheCar() {
    serviceCountState(); // Network work may have crossed midnight since loop start.
    if (!countsReady()) {
        // Preserve events during recovery without writing zero/fictitious dates.
        recordPendingVehicle();
        return;
    }
    /* COUNT SUCCESS */

    noCarTimer = millis();

    // RTC snapshot
    DateTime now = rtc.now();

    // Build a fresh timestamp string (do NOT use now.toString here)
    char timeBuf[25];
    snprintf(timeBuf, sizeof(timeBuf),
             "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());

    // Serial log (no temp/humidity here either if you don't care)
    Serial.print(timeBuf);
    Serial.print(", EnterDailyTotal (before increment) = ");
    Serial.println(totalDailyCars);

    // Increase count for every car going through car counter regardless of time
    totalDailyCars++;

    // Increment hourly car count
    int currentHour = now.hour();
    hourlyCount[currentHour]++;

    // Construct the MQTT topic dynamically
    char topic[60];
    snprintf(topic, sizeof(topic), "%s%02d", MQTT_PUB_CARS_HOURLY, currentHour);

    // Increase Show Count only when show is open
    recordOfficialVehicle();

    // The camera's green indication means the count increment succeeded.
    // Do this before synchronous SD/export work can delay visible feedback.
    digitalWrite(redArchPin,LOW);digitalWrite(greenArchPin,HIGH);

    // Commit this vehicle as one dated daily/show/hourly checkpoint before
    // MQTT or the web-facing files can delay persistence.
    persistCountState();
    // Reporting continues if SD fails; countPersistence identifies durability.
    publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars), true);
    publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars), true);
    publishMQTT(topic, String(hourlyCount[currentHour]), true);
    saveDailyTotal();
    saveShowTotal();
    saveHourlyCounts();

    Serial.print(F("TotalDailyCars after increment: "));
    Serial.println(totalDailyCars);

    // Build the current recording-year log path.
    String enterLogPath = String(seasonFolder) + fileName6;

    // Open file for writing Car Data
    myFile = recordingOpen(enterLogPath.c_str(), FILE_APPEND); // EnterLog.csv in season folder
    if (myFile) {
        // NEW CSV FORMAT:
        // DateTime, TimeToPass_ms, EnterDailyTotal, AB_Follow_ms, TimeBetweenCars_ms

        myFile.print(timeBuf);
        myFile.print(", ");
        myFile.print(timeToPassMS);        // A->B broken -> B clear (set in CAR_DETECTED)
        myFile.print(", ");
        myFile.print(totalDailyCars);      // EnterDailyTotal
        myFile.print(", ");
        myFile.print(abFollow_ms);         // A->B follow time (Beam 2 first high)
        myFile.print(", ");
        myFile.println(timeBetweenCars_ms); // raw millis between cars

        myFile.close();

        Serial.print(F(" Car "));
        Serial.print(totalDailyCars);
        Serial.println(F(" Logged to SD Card."));

        // Only car-related MQTT here; env is handled by 10-minute task
        publishMQTT(MQTT_PUB_TIME, timeBuf);
        publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars));
        start_MqttMillis = millis();

    } else {
        Serial.print(F("SD Card: Cannot open the file: "));
        Serial.println(fileName6);
    }
}

namespace {
traffic::PassagePolicy passage;
bool passageDebug=false;
uint32_t passageSequence=0;
int passageCountBefore=0;
String passageStartedLocal;
bool passageAlarmReported=false;
bool passageRedShown=false;
void reportPassage(const char* kind,int after) {
    if(!passageDebug || !mqttOnline()) return;
    // The debug record is MQTT-only; never enqueue or append diagnostic SD data.
    char message[768];
    snprintf(message,sizeof(message),
        "{\"event\":%lu,\"kind\":\"%s\",\"boot\":\"%s\",\"startLocal\":\"%s\","
        "\"endLocal\":\"%s\",\"order\":\"%s\",\"decision\":\"%s\",\"reason\":\"%s\","
        "\"aMs\":%lu,\"bMs\":%lu,\"overlapMs\":%lu,\"maxOverlapMs\":%lu,"
        "\"beamMinMs\":%lu,\"detectMs\":%lu,\"before\":%d,\"after\":%d}",
        static_cast<unsigned long>(passageSequence),kind,bootTimestamp.c_str(),passageStartedLocal.c_str(),
        getRtcTimestamp().c_str(),passage.order,
        passage.finished ? (passage.accepted ? "ACCEPT":"REJECT"):"PENDING",passage.reason,
        static_cast<unsigned long>(passage.aMs),static_cast<unsigned long>(passage.bMs),
        static_cast<unsigned long>(passage.overlapMs),static_cast<unsigned long>(passage.maxOverlap),
        static_cast<unsigned long>(passage.beamMinimum),static_cast<unsigned long>(passage.overlapMinimum),
        passageCountBefore,after);
    if(mqtt_client.getBufferSize()<1024 && !mqtt_client.setBufferSize(1024)) return;
    publishMQTT((String(topic_base_path)+"/Sensors/passageEvent").c_str(),String(message),false);
}
}

void publishPassageDebugState() {
    publishMQTT((String(topic_base_path)+"/State/passageDebug").c_str(),passageDebug ? "ON":"OFF",true);
}
void resetPassageDebugConnection() {
    // PubSubClient does not expose retain flags. Clear old commands BEFORE
    // subscribing so a retained ON cannot silently re-enable a new connection.
    passageDebug=false;
    publishMQTT((String(topic_base_path)+"/Config/passageDebug").c_str(),"",true);
    publishMQTT((String(topic_base_path)+"/Sensors/passageEvent").c_str(),"",true);
    publishPassageDebugState();
}
bool handlePassageDebugCommand(const char* topic,const char* message) {
    if(String(topic)!=String(topic_base_path)+"/Config/passageDebug") return false;
    if(strcmp(message,"ON")==0) passageDebug=true;
    else if(strcmp(message,"OFF")==0) passageDebug=false;
    else return true;
    if(!passageDebug) publishMQTT((String(topic_base_path)+"/Sensors/passageEvent").c_str(),"",true);
    publishPassageDebugState();
    if(passageDebug) publishMQTT((String(topic_base_path)+"/Sensors/passageEvent").c_str(),
        "{\"event\":0,\"kind\":\"SESSION\",\"decision\":\"WAITING\"}",false);
    return true;
}

void detectCar() {
    uint32_t now=millis();
    bool rawA=digitalRead(firstBeamPin),rawB=digitalRead(secondBeamPin);
    // Preserve the existing 50 ms debounce independently for each beam.
    if(rawA!=stableFirstBeamState) {
        if(now-lastFirstBeamChangeTime>debounceDelay) {
            stableFirstBeamState=rawA;lastFirstBeamChangeTime=now;
            publishMQTT(MQTT_PUB_BEAM_A_STATE,String(rawA),true);
        }
    } else lastFirstBeamChangeTime=now;
    if(rawB!=stableSecondBeamState) {
        if(now-lastSecondBeamChangeTime>debounceDelay) {
            stableSecondBeamState=rawB;lastSecondBeamChangeTime=now;
            publishMQTT(MQTT_PUB_BEAM_B_STATE,String(rawB),true);
        }
    } else lastSecondBeamChangeTime=now;
    firstBeamState=stableFirstBeamState;secondBeamState=stableSecondBeamState;
    passage.update(firstBeamState,secondBeamState,now,minActivationDuration,waitDuration);
    if(passage.started) {
        ++passageSequence;passageCountBefore=totalDailyCars;
        passageStartedLocal=getRtcTimestamp();passageAlarmReported=false;passageRedShown=false;
        firstBeamTripTime=now;
    }
    // Restore the camera-visible qualification indication, before storage/network work.
    if(passage.qualified && !passageRedShown && !passage.finished) {
        passageRedShown=true;stopIdleArchPattern();
        digitalWrite(greenArchPin,LOW);digitalWrite(redArchPin,HIGH);
    }
    // Alarm reports a blocked passage but never resets it or counts it twice.
    if(passage.active && now-passage.start>=carCounterTimeout && !passageAlarmReported) {
        passageAlarmReported=true;carStuckAlarmActive=true;
        publishMQTT(MQTT_PUB_ALARM,"ALARM_ENTER_STUCK",true);
        reportPassage("BLOCKED",totalDailyCars);
    }
    if(passage.finished) {
        if(carStuckAlarmActive) {carStuckAlarmActive=false;publishMQTT(MQTT_PUB_ALARM,"CLEAR",true);}
        if(passage.accepted) {
            abFollow_ms=passage.followMs;timeToPassMS=now-passage.start;
            timeBetweenCars_ms=now-lastCarDetectedMillis;
            lastCarDetectedMillis=now;
            // Show count acceptance immediately; synchronous SD verification follows.
            noCarTimer=now;stopIdleArchPattern();
            countTheCar(); // Green is set only after its dated count increment.
            publishMQTT(MQTT_PUB_BEAM_AB_MS,String(abFollow_ms));
            publishMQTT(MQTT_PUB_BEAM_B_BROKEN_MS,String(passage.bMs));
            publishMQTT(MQTT_PUB_TTP,String(timeToPassMS));
            publishMQTT(MQTT_PUB_BETWEENCARS_MS,String(timeBetweenCars_ms),true);
            publishMQTT(MQTT_COUNTER_LOG,"Car Counted Successfully!!");
        }
        reportPassage("COMPLETE",totalDailyCars);
    }
    // Compatibility state is observational; unqualified passages do not drive arches.
    currentCarDetectState=passage.active ? (firstBeamState && secondBeamState ? BOTH_BEAMS_HIGH:FIRST_BEAM_HIGH):WAITING_FOR_CAR;
}

// Only a qualified vehicle pauses the idle pattern, not raw beam activity.
bool vehicleArchFeedbackActive() { return passage.qualified && !passage.completed; }
