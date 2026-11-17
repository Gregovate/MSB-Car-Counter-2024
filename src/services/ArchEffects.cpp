#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.
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

// Idle effects are independent of official show-count eligibility.
void serviceIdleArchPattern() {
    static bool patternActive=false;
    unsigned long now=millis();
    bool clear=currentCarDetectState==WAITING_FOR_CAR && !carStuckAlarmActive &&
        !stableFirstBeamState && !stableSecondBeamState &&
        digitalRead(firstBeamPin)==LOW && digitalRead(secondBeamPin)==LOW;
    if(!clear) {
        noCarTimer=now; // A partial/blocked passage also interrupts inactivity.
        if(patternActive) {
            digitalWrite(redArchPin,LOW);
            digitalWrite(greenArchPin,HIGH);
        }
        patternActive=false;currentPatternState=INITIAL_OFF;patternModeMillis=now;
        return;
    }
    if(now-noCarTimer<30000UL) return;
    patternActive=true;
    playPattern();
}
