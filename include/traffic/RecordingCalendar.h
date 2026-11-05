#pragma once
#include "Platform.h"
#include "RecordingPolicy.h"

int recordingYear();
void initializeRecordingCalendar();
void loadRecordingCalendarFromSD();
void updateRecordingCalendar();
void applyRecordingCalendar(); // device adapter: paths + existing show date fields
void publishRecordingCalendar();
bool acceptShowStartDate(const String &date);
DateTime recordingShowStart();
int recordingDaysRunning();
bool recordingShowSeasonActive();
bool recordingPathWritable(const String &path, bool mkdirOnly = false);
File recordingOpen(const String &path, const char* mode = FILE_READ);
bool recordingMkdir(const String &path);
bool recordingRemove(const String &path);
bool recordingRename(const String &from, const String &to);
void noteNetworkClock(int year);

bool recordingWebWritable(const String &path);
