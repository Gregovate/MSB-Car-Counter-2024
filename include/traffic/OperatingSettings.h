#pragma once
#include "OperatingPolicy.h"
void initializeOperatingSettings();
void publishOperatingSettings();
bool handleOperatingCommand(const char* topic,const char* message);
void serviceShowSchedule();
bool officialShowNow();
int effectiveShowEnd();
int effectiveExtension();
void recordOfficialVehicle();
bool writeOfficialSummaries();
void applyOperatingDetector(int alarmMs,int detectMs);
const char* summaryExportStatus();
bool officialSeriesCanStart();
int operatingSeasonEndDate();
int operatingDownDate();
