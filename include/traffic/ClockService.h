#pragma once
#include "RuntimeState.h"

// Service interface; definitions are compiled once in src/services.
String getRtcTimestamp();
void SetLocalTime();
int computeDaysRunning();
int determineSeasonYear();
void resetHourlyCounts();
void timeTriggeredEvents();
