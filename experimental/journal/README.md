# OTA-preserving traffic journal — engineering prototype

Status: HOST-TESTED COMPONENTS ONLY. Not connected to detector/count restoration,
not enabled by setup(), not a flashable release, and not deployed. Existing .17
Car/.13 Gate do not acquire this capability by fetching the branch.

## Capacity and OTA
Repository platformio.ini has no partition override. Official PlatformIO v6.9.0
az-delivery-devkit-v4 manifest specifies 4 MiB flash. Arduino-ESP32 2.0.17
(default.csv, matching the operator's recorded 2.0.17 framework) defines:

| Partition | Offset | Bytes |
|---|---:|---:|
| nvs | 0x9000 | 20,480 |
| otadata | 0xe000 | 8,192 |
| app0 | 0x10000 | 1,310,720 |
| app1 | 0x150000 | 1,310,720 |
| spiffs | 0x290000 | 1,441,792 |
| coredump | 0x3f0000 | 65,536 |

Sources: https://github.com/platformio/platform-espressif32/blob/v6.9.0/boards/az-delivery-devkit-v4.json
and https://github.com/espressif/arduino-esp32/blob/2.0.17/tools/partitions/default.csv
Installed partition tables have NOT been read from either device. Defaults are
not a substitute for runtime verification. No partition table change is proposed.

Use at most the first 1 MiB of the existing data partition, in two 512 KiB banks.
Each immutable 320-byte record contains a 64-byte event, full 224-byte count
snapshot, schema/sequence, checksum and last-written commit marker. 1,300 records
use 416,000 bytes. Each bank holds 1,625 records after a 4 KiB header; total 3,250.
The other 384 KiB of the default data partition is outside this adapter's range.
Both OTA slots, OTA selection data, NVS and coredump remain untouched.

The runtime adapter rejects absent OTA slots, overlaps, too-small/non-DATA/
non-SPIFFS/encrypted partitions and out-of-range writes. The store never formats flash and refuses unrecognized existing data in its reserved banks.
It
never overwrites consumed/torn slots and explicitly reports full capacity.
Reclamation requires byte-exact SD verification of every committed record in the
older bank and a newer durable snapshot in the other bank. No erase is allowed
for the bank holding the newest snapshot. Interrupted erase/header recovery is
still an integration gate; this is not ready for unattended deployment.

## Executable evidence
Run from either repository root:

    g++ -std=c++17 -Wall -Wextra -Werror -Iinclude experimental/journal/journal_test.cpp -o /tmp/journal-test
    /tmp/journal-test
    g++ -std=c++17 -Wall -Wextra -Werror -Iinclude experimental/journal/sd_archive_test.cpp -o /tmp/archive-test
    /tmp/archive-test

Tests cover 1,300 records, cold reboot, 313 record-write power-cut positions,
consuming torn slots without overwriting them, filling both banks, explicit full
refusal, unknown-data/read-failure/corruption protection and OTA layout guards.
Tests also cover refusing reclamation with a conflicting archive or newest
snapshot, then reclaiming the verified older bank, appending and cold reopening.
Replay is checked after a 600-record SD outage and on repeated replay. Conflicting
archive identities are preserved, never overwritten. The filesystem archive
adapter is separately tested at all 320 short-write positions plus failed
readback/rename, retry, repeated replay and recording-year protection.

These are deterministic native simulations; they do not certify physical flash
power-loss behavior or SDK timing. The ESP-IDF runtime adapter has not been
compiled against the operator's licensed complete firmware environment here.

## Integration gates still open
1. Read-only runtime partition inventory. Do not claim free/unused data space
   from the partition size alone. Refuse unknown content without formatting it.
2. Feed every accepted vehicle and every reset/baseline/day rollover into the
   journal with a single idempotent identity. Capturing only vehicle counts is
   insufficient: a reboot after a reset can otherwise resurrect earlier totals.
3. Persist timestamps/timing before SD/network activity; restore the latest
   complete journal snapshot, reconcile SD independently, and never add a replayed
   event to already restored totals. Existing SD CountRecord sequence and journal
   sequence are separate and must not be compared as the same clock.
4. Clock-invalid/no-baseline detections need an explicit undated record variant.
   The prototype currently requires a valid count snapshot and must NOT be used
   to fabricate calendar dates. It does not replace the deployed NVS pending count.
5. Safe bank reclamation only after independently verified SD archival and a
   preserved newer recovery snapshot; full-device power-cut tests at bank-header
   initialization/rotation are required. The current prototype fails
   closed on interrupted headers, preserving bytes for inspection.
6. Prevent filesystem OTA uploads from overwriting the journal while retaining
   firmware OTA. Verify the licensed ElegantOTAPro interface; do not guess an API.
7. Flash writes may stall cache-backed code on ESP32. Do not claim nonblocking
   detection from a task alone. Protect sampled edges with a validated IRAM-safe
   capture/peripheral path and bound overflow explicitly before deployment.
8. Persist/expose journal-full, append failure, archive conflict, undated data and
   sample overflow as separate actionable health states, including the OLED.

No new deployment is requested for this prototype. Keep currently healthy
counters running. Hardware low-level SD errors remain an independent open cause.
