# Firmware Refactoring Analysis — Part 2 checkpoint
Date: 2026-11-24 (America/Chicago).
Status: documentation checkpoint; production cutover implementation is NOT released.

## Start here
Read this document, docs/OPERATING_CONTRACT.md, repository AGENTS.md if present, README.md, and the shared-module verification documentation before changing code. Use BOTH repositories as permanent memory:
- Gregovate/MSB-Car-Counter-2024
- Gregovate/MSB-Gate-Counter-2025
Working branch in both: fix/traffic-reliability-20261122.

Workflow: assistant implements, verifies, and pushes code; Greg pulls, compiles locally, and flashes each ESP32. Preserve licensed local OTA dependencies, OTA firmware slots, Fritzing files, and historical data. Do not ask Greg to perform engineering investigation that can be done from source. Do not claim a build or hardware test that was not performed. Keep common modules identical where behavior is shared; document legitimate device differences. Document every release and unfinished item in BOTH repositories.

## Last confirmed installed firmware
- Car: 25.12.03.2-modular-test.30.
- Gate: 25.12.28.0-modular-test.23.
Greg confirmed both compiled with ZERO warnings and both were flashed.
These installed versions are NOT the requested production cutover release.

Code release commits before the operating-contract documentation:
- Car: 0b4fc5a2413dba1879572b97c83f1487262aaafa.
- Gate: 08a664e228d09b014e5186915a3dc75b17714af1.
Operating-contract commits:
- Car: 11677b550c53ac3735002b596680a5c42c33b47c.
- Gate: 0cd24056a35586efcdf981bda5d08112c6bc57b4.

## Confirmed decisions — do not ask again
The authoritative details are in OPERATING_CONTRACT.md.
Everything currently collected is TEST data. Public opening is Friday November 27, 2026, 5 PM local. Greg will set the HA show start date to November 27. The requested transition must archive tests separately and begin with zero production counts, event traffic logs, recovery state, and report rows. Never wipe on ordinary reboot, duplicate HA publication, or reconnect.
Production traffic counts/logs belong ONLY to approved show windows. Pre-show counts are informational and excluded from production occupancy.
HA owns times, offsets, dates, and extensions. Start is always 5 PM. Car baseline must zero at 5 PM; Gate zeros later according to its accepted HA offset. Preserve Gate opening +10 minutes and closing +25 minutes unless HA changes them, including authorized extension (normally zero).
Minimum traverse time is 13 minutes at 5 mph. This explains delayed Gate handling; it does NOT authorize hard-coding a 13-minute offset. Closing grace permits draining; never invent exits or force a remaining occupancy to zero.
Latest observed Car reset offset was -1 minute and closing grace +10. Exact 5 PM reset and exact entrance close require review of HA's existing Car helpers (0 offsets); do not silently override HA in firmware.
Friday/Saturday, especially Saturday, and December 23 are largest nights. Current light tests do not establish capacity.
Car exterior temperature is official. Gate enclosure temperature is an unofficial backup.
Preserve HA unique IDs; shared environment topic is Env/temp. One supplied Gate humidity YAML had an accidental mmsb/ prefix; correct to msb/ when editing that configuration.

## What is already working
Flash journal, verified SD archive, count persistence, beam capture diagnostics, OTA preservation, and provisioning have been deployed. Journal reported about 3247–3249 free records; no beam capture drops were reported in supplied snapshots. This is evidence from light testing, not a busy-night stress guarantee.
Shared DHT compatibility decoding handles the observed negative-temperature encoding variant. Raw diagnostic values near -5860 F are pre-correction values; Env/temp contains the corrected value. Both sensors returned plausible subfreezing readings.
Car .29 added persisted, date-qualified rolling completed-hour temperatures; Gate uses matching interfaces with backup semantics. Reports support CSV, print/PDF, and copying for nightly email.
File manager defaults to the active season, exposes storage/recovery status and reports, and has guarded cleanup. Do not remove nightly reports.
Common SeasonFolder char[32], warning fixes, and removal of unused Arduino-DS3231 dependency were verified in .30/.23; actual RTC driver remains RTClib.
Last recorded native suite: Car 43 tests / Gate 40 tests; portability 3 each. Shared manifest reached 87 entries after operating-contract documentation. Re-run current tests rather than assume these counts remain unchanged.

## Immediate unfinished work — engineering blocker
Installed .30/.23 still increment raw counts and write passage logs outside the official window. acceptShowStartDate only changes the calendar; it DOES NOT perform the requested all-storage test-to-production transition.
Finish and fault-test:
1. Idempotent, recoverable production-session transition across flash journal, pending recovery NVS, SD snapshots, CSV/event logs, and report exports.
2. Strict show-window count/log qualification while preserving beam sensing, alarms, and protection during flash operations. Keep Gate HA delays/extensions.
3. Preserve existing verified test journal evidence separately; do not re-import it into a fresh production folder on reboot.
4. Correct occupancy baseline across the Car 5 PM reset and later Gate reset; never subtract yesterday's exits from today's new entries.
5. Hourly entrance/exit/net/end occupancy and peak occupancy analysis. These are not yet present as complete Gate report columns.
6. Document and release tested firmware, with local build/flash instructions and explicit validation. Do not label this checkpoint as ready-to-flash firmware.

## Unreleased local experiments — NOT authoritative source
The prior workspace had experimental ProductionSessionPolicy.h / ProductionSession.h and JournalStore kind-7 SESSION_START edits, plus tools/report_occupancy.py and tests/test_occupancy_report.py. These were NOT committed, integrated, or released at this checkpoint. Do not assume they exist in a fresh clone or are safe.
Proposed design: CRC-protected NVS receipt with phases Requested -> BaselineCommitted -> TestDataArchived -> Ready; monotonic journal sequence and a session-start marker rather than erasing/repartitioning flash; scope pending recovery to session floor; archive old season directory to a sibling TEST archive; route old journal records to that archive; gate normal SD projections while transition is pending. Verify crash recovery at EVERY boundary and do not auto-wipe merely because an existing calendar already says November 27.
Occupancy prototype used verified journal records from both devices and official qualification, date-qualified baselines, uncertainty on missing records/negative balance, and offline CSV/HTML. It was not published or integrated into firmware web reports.

## Verification and release safeguards
Use fresh remote branch heads and inspect changes before edits. Preserve Greg's newer documentation/Fritzing additions. Do not force-push or substitute an old workspace snapshot for current source.
Run tools/verify_modular.py with peer verification and relevant native tests. Update shared hashes/function behavior records only for deliberate reviewed changes; do not bless unrelated divergence.
Current resumed workspace lacked PlatformIO/ESP SDK/private OTA dependencies. Native g++/Node tests were available. Greg performs the real local firmware build and flash.
Do not modify live HA schedules or delete live data as part of source work. Do not promise the physical SD/power fault is solved: logs showed real card command failures on two cards, with possible startup power involvement but no proven electrical diagnosis.

## Relevant repository references
- docs/OPERATING_CONTRACT.md
- docs/Traffic_Journal_Release_20261123.md
- docs/Count_Persistence.md
- docs/SD_Investigation_20261123.md
- docs/Journal_Provisioning_20261123.md
- docs/Operator_Deployment_20261123.md
- docs/Storage_UI_and_PreShow_Cleanup_20261123.md
- docs/Nightly_Reports_and_Binary_Cleanup_20261124.md
- docs/Rolling_Hourly_Temperatures_20261124.md
- docs/Environment_MQTT_Layout_20261123.md
- docs/Configuration_Ownership.md
- docs/Firmware_History.md

## Suggested first message in the new thread
Continue MSB Firmware Refactoring Analysis Part 2. Read docs/Checkpoint_2026-11-24_Part_2.md and docs/OPERATING_CONTRACT.md in BOTH Gregovate counter repositories on fix/traffic-reliability-20261122. Finish the documented production-session cutover, show-only recording, and occupancy work. You implement and push; I compile and flash locally. Do not treat unfinished work as released.
