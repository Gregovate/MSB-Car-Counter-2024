#include "traffic/Firmware.h"

// Extracted from the production baseline; behavior fixes are separate changes.




int computeDaysRunning() {
    return recordingDaysRunning();
}

int determineSeasonYear() {
    return recordingYear();
}



void timeTriggeredEvents() {
    // One shared HA-configured scheduler, independent of MQTT availability.
    serviceShowSchedule();
}

