# Storage, file-manager and pre-show cleanup — 2026-11-23

Release: Car .26 / Gate .19. Both use identical FileManagerService.cpp,
FileManagerPage.h and data/index.html. The primary page is embedded in firmware;
no SD upload or filesystem flash is needed for this page. Do not use PlatformIO
uploadfs, erase_flash or a partition upload. Build/flash firmware normally. OTA
slots and existing journal layout are unchanged.

## What is self-managing

The existing data partition contains a raw journal, not a mounted LittleFS/SPIFFS
filesystem. Two 512 KiB banks hold 3250 records in total; a count snapshot, reset,
or correction also uses a record. The observed 3249 free records reflects one
occupied record. Do not equate free records exactly with vehicles remaining.

Accepted traffic is committed locally before MQTT publication. With SD available,
serviceTrafficJournal archives immutable 320-byte records under:
- /CC/<recording year>/TrafficJournal/0000000001.bin etc.
- /GC/<recording year>/TrafficJournal/0000000001.bin etc.

An archive record is read back and compared before it is treated as verified.
Flash banks are reclaimed only after their records are verified on SD and a newer
snapshot remains protected. SD return triggers automatic archival/reuse; MQTT is
not required for local recording. A sustained SD outage can exhaust flash. The
journal is not unlimited, and SD archive files are not automatically deleted.
Never format/reset the journal to reconcile totals or clear pending events.
Both installed devices were reported OK/OK with 3249 free records on Nov23.

## What the new page does

Opening / defaults to the active recording folder supplied by recordingYear and
seasonFolder, not a browser clock or hard-coded 2026. Clicking Current season
returns there. Root / settings and Journal archive are explicit navigation buttons.
Folder names now appear in listings. Status refreshes every ten seconds and is
available even when SD is offline; file operations need SD. It shows journal and
archive health, free journal records, SD availability, count recovery readiness,
beam capture state/drop count, daily count and show total. This is operational
status, not an event analytics dashboard. /storageStatus is read-only and does
not initialize, erase, archive or recover storage from an HTTP callback.

The counts are read-only on this page; there is no clear-journal or reset-counts
button. Supporting summary pages remain SD-served. Summary CSV downloads use the
active season. Shared source/embedded-page parity is enforced by verification.

## Why recent files are in two places

| Location/files | Purpose | Cleanup decision |
| --- | --- | --- |
| /recording-calendar.a and .b | Redundant recording-calendar settings | Keep in root |
| /operating-settings.a and .b | Redundant accepted HA schedule/tuning | Keep in root |
| /CC/2026 or /GC/2026 | Counts, traffic CSVs and official summaries | Keep active |
| CountState.a/.b | Redundant count checkpoints | Keep; web-protected |
| ClosedCounts-YYYYMMDD.bin (224 bytes) | Closed daily count snapshots | May delete after backup and a verified newer current-day checkpoint |
| *.previous / *.pending | Verified export fallback or interrupted work | Preserve; web-protected |
| TrafficJournal/*.bin (320 bytes) | Verified immutable event archive | May delete after backup only when dated before today and retired from both live flash banks |
| /data | Supporting web pages/assets | Keep |
| /CC261123.zip, /CC2025.zip, /CC22025b4show.zip, /GC2024.zip, /GC2025.zip | Operator backup archives | Download and verify off-device before considering removal |
| /EnterLog.csv dated 2025 in the screenshot | Historical root log | Archive separately; do not merge into current-year log |
| /DeleteMe.txt | Unknown legacy helper | Inspect content first; filename alone does not establish safe deletion |

These root settings are intentionally independent of the season folder. Their
recent timestamps do not mean that traffic is being written into two active logs.
The screenshot's seasonal EnterLog.csv is the current traffic log; the root
EnterLog.csv has a 2025 timestamp. Preserve evidence rather than assuming duplicates.

## Thursday archive / Friday opening

1. Keep the existing full SD backup. Download new root/season data and journal
   archive before cleanup; validate downloaded sizes/content and keep copies of
   settings/checkpoints as well as CSVs. An old ZIP does not include later events.
2. Keep active settings, recovery/checkpoint files and journal archives on SD.
   At 29+ GB available, space pressure does not justify deleting recovery data.
3. Only consider redundant old backup ZIPs/historical root files after off-device
   verification. This release performs no deletions, moves, merges or count resets.
4. Archiving test data does not remove mini-test totals from current counts.
   If show totals must start at zero, use the existing explicit count-correction
   procedure before opening; never achieve it by deleting files. Preserve 24/7
   daily traffic and the one historical unassigned recovery event for review.
5. Confirm fresh heartbeat plus countPersistence, journalHealth, journalArchive
   and SD health OK, and beamCaptureDropped=0 on both counters before opening.
   This does not prove every historical passage was counted; known downtime
   passages still require separate evidence-based reconciliation.

No hardware action or manual journal maintenance is required while healthy.
The reported unusually slow Gate restart remains unexplained; these UI changes
must not be presented as a fix for that observation.
