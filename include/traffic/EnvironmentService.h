#pragma once
#include "RuntimeState.h"

// Service interface; definitions are compiled once in src/services.
void averageHourlyTemp();
void readTempandRH();

// Reconnect/periodic publication uses validity, not internal startup sentinels.
void publishEnvironmentState();
void serviceEnvironmentHour();
bool showTemperatureAverage(float& average);

const char* environmentPersistenceStatus();

void publishEnvironmentHourlyState();
