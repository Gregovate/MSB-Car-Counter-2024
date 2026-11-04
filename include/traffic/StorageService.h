#pragma once
#include "RuntimeState.h"

// Service interface; definitions are compiled once in src/services.
void checkSdHealth();
void publishSdDiag_(const char* step, const char* file, const char* mode, const char* err);
void ensureSeasonFolderExists();
void getDailyTotal();
void getShowTotal();
void getDayOfMonth();
void getDaysRunning();
void getHourlyData();
void saveDailyTotal();
void saveShowTotal();
void saveDayOfMonth();
void saveDaysRunning();
void saveHourlyCounts();
void saveDailyShowSummary();
void getSavedValuesOnReboot();
void checkAndCreateFile(const String &fileName, const String &header = "");
void createAndInitializeHourlyFile(const String &fileName);
void initSDCard();
