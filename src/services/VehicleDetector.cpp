#include "traffic/Firmware.h"
#include "traffic/PassagePolicy.h"

// Extracted from the production baseline; behavior fixes are separate changes.
void countTheCar() {
    serviceCountState(); // Captured event date owns rollover, even after a stall.
    serviceEventReset(); // Apply an armed logical reset before this event increments.
    // A season transition must finish before we change its durable baseline.
    if (productionBlocked() || (!productionActive() && productionDateMatches()))return;
    if (productionActive() && !countEventClockValid()) {
        noteUnqualifiedClock();
        return;
    }
    // Keep the live counter and detector feedback running between shows.
    // Only eligible passages get a vehicle record in the season archive.
    const bool recordPassage = !productionActive() || officialShowNow();
    if (recordPassage) activateTrafficVehicleEvent();
    if (!countEventCanApply()) {
        // Preserve events during recovery without writing zero/fictitious dates.
        if (recordPassage) recordPendingVehicle();
        return;
    }
    /* COUNT SUCCESS */

    noCarTimer = millis();

    // RTC snapshot
    DateTime now = countEventTime();

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
    if (recordPassage) hourlyCount[currentHour]++;

    // Construct the MQTT topic dynamically
    char topic[60];
    snprintf(topic, sizeof(topic), "%s%02d", MQTT_PUB_CARS_HOURLY, currentHour);

    // Increase Show Count only when show is open
    if (recordPassage) recordOfficialVehicle();

    commitCountJournal(); // Only internal flash; no SD or network wait here.

    // The camera's green indication means the count increment succeeded.
    // Do this before synchronous SD/export work can delay visible feedback.
    digitalWrite(redArchPin,LOW);digitalWrite(greenArchPin,HIGH);


    // Publish the accepted RAM count immediately. SD durability is reported
    // separately; a publication does not claim checkpoint success.
    publishMQTT(MQTT_PUB_ENTER_CARS, String(totalDailyCars), true);
    publishMQTT(MQTT_PUB_SHOWTOTAL, String(totalShowCars), true);
    publishMQTT(topic, String(hourlyCount[currentHour]), true);
    publishMQTT(MQTT_PUB_TIME,timeBuf);
    persistCountState(); // SD projection follows immediate current-state publication.
    saveDailyTotal();
    if (!recordPassage) return;
    saveShowTotal();
    saveHourlyCounts();

    Serial.print(F("TotalDailyCars after increment: "));
    Serial.println(totalDailyCars);

    // Build the current recording-year log path.
    String enterLogPath = String(seasonFolder) + fileName6;

    // Open file for writing Car Data
    myFile = sdAvailable ? recordingOpen(enterLogPath.c_str(), FILE_APPEND) : File(); // EnterLog.csv in season folder
    if (myFile) {
        // NEW CSV FORMAT:
        // DateTime, TimeToPass_ms, EnterDailyTotal, AB_Follow_ms, TimeBetweenCars_ms

        myFile.print(timeBuf);
        myFile.print(", ");
        myFile.print(timeToPassMS);        // A->B broken -> B clear (set in CAR_DETECTED)
        myFile.print(", ");
        myFile.print(productionActive() ? officialDailyPassages() : totalDailyCars);      // EnterDailyTotal
        myFile.print(", ");
        myFile.print(abFollow_ms);         // A->B follow time (Beam 2 first high)
        myFile.print(", ");
        myFile.println(timeBetweenCars_ms);

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

namespace {uint32_t bothClearAt=0,clearBeforePassage=0;bool wasBothClear=false;}
void processBeamSample(uint32_t now,bool rawA,bool rawB) {
    setCountEventMillis(now);
    if(!rawA && !rawB && !wasBothClear)bothClearAt=now;
    wasBothClear=!rawA && !rawB;
    if(rawA!=stableFirstBeamState) publishMQTT(MQTT_PUB_BEAM_A_STATE,String(rawA),true);
    if(rawB!=stableSecondBeamState) publishMQTT(MQTT_PUB_BEAM_B_STATE,String(rawB),true);
    stableFirstBeamState=rawA;stableSecondBeamState=rawB;
    firstBeamState=stableFirstBeamState;secondBeamState=stableSecondBeamState;
    passage.update(firstBeamState,secondBeamState,now,minActivationDuration,waitDuration);
    if(passage.started) {
        clearBeforePassage=now-bothClearAt;
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
            setTrafficVehicleEvent(now,timeToPassMS,abFollow_ms,passage.bMs,timeBetweenCars_ms,clearBeforePassage);
            countTheCar(); // Green is set only after its dated count increment.
            publishMQTT(MQTT_PUB_BEAM_AB_MS,String(abFollow_ms));
            publishMQTT(MQTT_PUB_BEAM_B_BROKEN_MS,String(passage.bMs));
            publishMQTT(MQTT_PUB_TTP,String(timeToPassMS));
            publishMQTT(MQTT_PUB_BETWEENCARS_MS,String(timeBetweenCars_ms),true);
            if (totalDailyCars!=passageCountBefore) publishMQTT(MQTT_COUNTER_LOG,"Car Counted Successfully!!");
        }
        reportPassage("COMPLETE",totalDailyCars);
    }
    // Compatibility state is observational; unqualified passages do not drive arches.
    currentCarDetectState=passage.active ? (firstBeamState && secondBeamState ? BOTH_BEAMS_HIGH:FIRST_BEAM_HIGH):WAITING_FOR_CAR;
}

// Occupancy owns the arches, even before qualification and after B-clear.
// An alarm never releases an unresolved passage to the idle animation.
bool vehicleArchFeedbackActive() {
    return passage.active || stableFirstBeamState || stableSecondBeamState ||
        digitalRead(firstBeamPin) || digitalRead(secondBeamPin) || carStuckAlarmActive;
}

void detectCar(){serviceBeamCapture(debounceDelay,processBeamSample);}
void resetBeamDetectorAfterLoss(){passage=traffic::PassagePolicy{};}
