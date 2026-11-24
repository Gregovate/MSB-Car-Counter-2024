#pragma once
#include "CountRecord.h"
#include "JournalStore.h"
// Count snapshots and event identities have separate sequences. No MQTT replay
// changes a count; SD archival copies immutable records only.
void initializeTrafficJournal();
const traffic::CountRecord* latestTrafficSnapshot();
bool appendTrafficSnapshot(const traffic::CountRecord& counts);
bool appendUndatedTraffic(uint32_t pending);
uint32_t journalPendingVehicles();
void serviceTrafficJournal();
void publishTrafficJournalHealth();
const char* trafficJournalHealth();
bool trafficJournalDurable();
void setTrafficVehicleEvent(uint32_t endMs,uint32_t passageMs,uint32_t followMs,uint32_t brokenMs,uint32_t betweenMs,uint32_t clearBeforeMs=0);

bool appendTrafficCorrection(const traffic::CountRecord& counts,uint32_t nonce);
uint32_t journalCorrectionNonce();
void carryJournalPending(uint32_t pending);

void activateTrafficVehicleEvent();

// Read-only status accessors; never open, erase or archive from HTTP callbacks.
const char* trafficJournalArchiveHealth();
uint32_t trafficJournalFreeRecords();

// Explicit post-backup removal of dated SD records already retired from flash.
bool removeRetiredJournalArchive(const String& path);

// The receipt is loaded before calendar/SD initialization on every boot.
void initializeProductionSession();
bool requestProductionSession();
bool productionBlocked();
bool productionActive();
bool productionRequested();
void serviceProductionSession();
void resetSessionRam();
bool productionCardMatches();
bool commitClosingCheckpoint();
void setupOccupancyRoutes();
void noteUnqualifiedClock();
bool productionCardRejected();
uint32_t productionStartDate();
