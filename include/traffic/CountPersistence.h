#pragma once
// Common durable count state; SD is independent of MQTT reporting.
bool countsReady();
void recordPendingVehicle();
bool restoreCountState();
bool persistCountState();
void serviceCountState();
bool resetCountsForShow();
bool countTopicPublishable(const char* topic);
void noteFreshCountFolder();
void publishCountHealth();
bool recoverHourlyExport(const String& path);
bool writeHourlyExport(const String& path, const String& content);
void reportHourlyExportStatus(const char* status);

extern unsigned int officialHourly[24];
extern bool officialCountsComplete;

bool countEventCanApply();

bool correctCountState(bool daily,uint32_t value,uint32_t nonce);

bool commitCountJournal();

// Explicit post-backup cleanup; rejects every active/recovery-required record.
bool removeObsoleteClosedCount(const String& path);
