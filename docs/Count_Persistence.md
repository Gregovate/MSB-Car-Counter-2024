# Count persistence and reboot recovery — 2026-11-04

## Status and evidence
Current source revision `.5` on `refactor/shared-firmware`; not uploaded yet.
Car `.4` licensed build passed (29.77 s) and its OTA page subsequently reported `.4` installed on November 5. Initial migration reported `RESTORE_FAILED_KEEP_FILES`; field diagnosis and correction are below. Gate has not been reported updated from `.2`.
The `.3` licensed local builds reported by Greg passed (Car 32.55 s, Gate 23.00 s); those builds do not include this change.
Greg observed today's Car count reach 40, disappear across a reboot, then a seven-car manual test (later eight) disappear. HA history preserves the reported changes; it does not prove which write completed or the exact cause of each drop.

Confirmed source defects in `.2`:
- Car deferred daily/show/hourly writes to `KeepMqttAlive`, leaving approximately a reporting interval of unsaved counts. Gate saved in each vehicle event.
- Boot could select 2026 using the RTC, then a stale show-date callback could change writes to 2025. The observed SD write diagnostic named `/CC/2025/ShowTotal.txt` while retained folder telemetry said 2026. Booting against the other folder can restore different totals. `.3` already fixes this path ownership defect.
- Recovery compared only day of month, conflated failed/malformed reads with defaults, and could write zero during recovery. Car's old new-day branch also omitted show-total restoration (fixed in `.3`).
- Show-reset completion existed only in RAM; reboot during the scheduled minute could reset again. The old pre-season reset also ran unconditionally; `.3` gates it by the show calendar.
- Both hourly exporters truncated the entire CSV in place and could build a replacement after a failed read.

Do not assert that the 30-second delay alone explains the 40 lost counts. The confirmed folder mismatch and SD errors are additional evidence. No previous-year files or official totals are reconstructed or modified.

## Shared implementation
`CountPersistence.cpp`, `CountPersistence.h`, and `CountRecord.h` are identical in both repositories and checked by the peer verifier.

Each accepted vehicle updates daily/show/hourly RAM state, then immediately commits one 124-byte checkpoint before reporting and legacy exports. This does not wait for MQTT, the heartbeat timer, or a network connection. The two alternating current-year files `CountState.a` and `CountState.b` contain a format marker, sequence, full YYYYMMDD date, daily total, show total, 24 hourly counts, show-reset date, and checksum. Writes are flushed, closed, reopened and byte-verified before the new slot is considered saved. A failed save leaves the last valid slot intact and retries from RAM every five seconds. This does not make a physically failing SD card infallible.

On reboot the highest-sequence valid copy is restored. Same date restores daily/show/hourly state. A later full date clears only daily/hourly state and preserves the annual show total; month/year are not inferred from day number. Runtime rollover uses the full date, not 23:59 or an exact second. A newly created year folder starts a new annual baseline. Show reset saves its date with zero, so subsequent vehicles survive another reboot in that reset minute. Hourly values remain separate from the daily show-window reset.

The legacy text and CSV files remain available to the web UI. Hourly replacement writes `.pending`, verifies it, renames the old file to `.previous`, then promotes the verified replacement. Startup recovers `.previous` before any empty-file initializer if interrupted between renames. The previous complete CSV is retained until the next verified replacement. Current-year internal checkpoint and hourly recovery files cannot be uploaded/deleted/renamed through the web manager. Historical paths retain `.3` write protection.

## First upgrade / limitations
- With no checkpoint, import only the current-year legacy files. Numeric counts and all dated hourly rows are validated; hourly totals are never summed to invent a daily total.
- Legacy empty show-total files are interpreted as the existing zero-initialized annual value. Daily count/day must be valid; a nonzero daily count without today's hourly row is rejected as ambiguous. This cannot prove historical legacy totals were complete.
- An annual folder freshly created by this boot can initialize zero. An existing ambiguous folder or damaged checkpoint causes recovery to stop without overwriting count files. Firmware continues diagnostics/detection; unverified RAM counts are not published as authoritative Cars totals. Inspection is required; there is no automatic reconstruction from older years.
- Installing `.4` does not transfer counts that `.2` misfiled under 2025 into 2026. It does not recover today's missing 40 or promise to repair today's history.
- MQTT remains reporting. The checkpoint is the recovery authority after migration. Editing legacy count files alone will not override it; existing HA count-set commands still explicitly change counts.
- Existing numeric HA reset commands, including any retained reset commands, retain their semantics. Audit those HA automations/topics separately before claiming every possible count reset is eliminated. Do not clear or publish reset commands during this test.
- Wi-Fi-blocking startup/reconnect, HA offset/tuning reconciliation, offline bag-sensor mode, end-of-show summary idempotency and full configuration persistence remain separate work. Per-vehicle SD latency needs physical traffic acceptance. No claim of complete offline hardware acceptance is made.
- Card loss after a successful restore leaves RAM counting/reporting active but durability degraded. An ordinary firmware update must not erase SD. A failed SD can still lose unsaved traffic; a successful flush/readback is bounded evidence, not a power-failure guarantee for the card controller.

## Diagnostics
| Topic suffix under each device | Meaning |
|---|---|
| `System/countPersistence` | `OK`, explicit `SAVE_FAILED_*`, `RESTORE_FAILED_*`, clock faults, or `RECOVERED_ONE_VALID_COPY_CHECK_SD` |
| `System/unrestoredDailyCount` | Partial RAM daily value while recovery is unresolved; not an official total; consult persistence status |
| `System/hourlyExport` | `OK`, `WRITE_VERIFY_FAILED`, `BACKUP_FAILED`, `REPLACE_FAILED`, `RECOVERY_FAILED`, or `READ_FAILED_KEEP_FILE` |

Persistence health is retained and repeated on heartbeat. Generic SD health `OK` does not override a count-persistence fault. Car's existing SD health check now verifies the test byte and can recover its availability flag after a transient connection failure.

## Validation
- 13 native Python/C++ tests passed per repository, including real count-service fault injection and execution of each real `countTheCar` function followed by immediate simulated reboot: 40→41 survives with matching show/hourly totals.
- Cases include torn save and retry, last valid copy recovery, seven counts surviving reset-minute reboot, different month with same day number, new-day/new-year startup, invalid clock, failed/malformed reads, protected 2025 data, Gate absolute paths, and interrupted hourly replacement.
- Strict preservation checks pass: Car 89 function bodies, Gate 94; 23 shared files match. Original unaffected behavior/config/GPIO/topic/state/enum baselines remain intact; changes are pinned in explicit behavior overrides.
- ESP32 compile/link checks pass with cached public dependencies and the explicit compile-only OTA interface stand-in. Car: 50,296 bytes RAM, 1,022,121 flash; Gate: 50,288 RAM, 1,008,121 flash. These artifacts must never be deployed. Greg must build with his licensed ElegantOTAPro source before any upload.
- Car licensed `.4` build/install is now evidenced above; Gate `.4` and both `.5` licensed builds/installations remain pending. Hardware persistence acceptance remains pending.

## Controlled hardware acceptance procedure
Owner: the respective counter repository; branch `refactor/shared-firmware`. Coordinate HA schedule policy with the governing Show Controls repository. One device and one step at a time.

1. Read-only: pull this source, verify the peer files and manual `.5` version, then run the local licensed build. Preserve `.vscode/extensions.json` edits. Do not upload compile-contract binaries.
2. Read-only: capture live firmware, heartbeat/time, counts, persistence/SD diagnostics and current-year files. Preserve existing end-of-season 2025 backups; do not rename or edit 2025 files. Car currently reports `.4` with migration blocked; Gate last reported `.2`. Verify live identities before upload.
3. Before upload, review baseline expectations: `.2` may have written current traffic under 2025; `.4` imports 2026 only. Do not label an expected folder migration as proof of a successful count restoration. Choose an idle period and agree the single-device firmware-only OTA action. No filesystem upload, SD format, factory reset, or count command.
4. After that agreed upload, inspect `.5` identity, current-year paths, full RTC date, `countPersistence` and hourly export status. On a restore fault stop further reset/upload actions and download the current-year evidence for inspection. Keep `.a`, `.b`, `.previous`, and `.pending` files intact. Do not revert to `.2` as a routine remedy: it can write into 2025.
5. When physical testing is possible, note starting daily/show/hourly values; pass a known number of vehicles; verify a completed checkpoint and matching exports. Perform one agreed reboot; require identical same-day values and no reset command. Verify failed-SD warning and restoration using a controlled test card/bench setup, not by interrupting the historical card remotely.
6. Verify midnight/full-date rollover and reboot within the show-reset minute on a disposable test setup. Do not change the production RTC/HA calendar to force this test.

The two-copy format protects the recovery path, but production acceptance remains pending until these checks pass.

## Field correction: zero legacy baseline — 2026-11-05

Observed Car files under `/CC/2026`: EnterTotal.txt is `0`, ShowTotal.txt is empty, DayOfMonth.txt is `4`; HourlyData.csv has an all-zero October 19 row and October 21 counts 1,1,2 at hours 12,13,14. There are no CountState files. On November 5, `.4` required the newest hourly row's day (21) to equal the day marker (4), unnecessarily rejecting the already-zero daily baseline. This was an upgrade-rule defect, not proof of a failed SD card.

`.5` permits this specific class of migration: daily count must parse as zero, all hourly rows must remain valid/current-year/not future, and the newest hourly row must precede today. The existing empty legacy show-total initializer remains zero. Older hourly rows remain history, not today's counts. Nonzero inconsistent totals, malformed/failed reads, damaged checkpoints and prior-year writes remain rejected. No file editing/reset command is needed to work around the defect.

The exact field fixture was reproduced failing on `.4`, then passed with the shared `.5` correction. It also verifies one newly counted vehicle survives reboot after migration, and a nonzero mismatched daily count still fails without modifying files. Both 13-test suites and peer verification pass. ESP32 compile-contract checks are recorded in Modular_Validation.md; licensed `.5` builds and hardware validation remain pending.

Before another upload, read `System/unrestoredDailyCount` on the running Car to capture any partial detections while `.4` recovery was blocked. A firmware reboot cannot retain that RAM-only value automatically. Do not invent a recovered baseline or alter 2025 history.
