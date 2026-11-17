# Coordinated traffic release .9 — park acceptance

Prepared 2026-11-05. Installed baseline: both devices .5. Releases .6–.8 were source-only and need not be installed separately. This release combines both firmware sources and private Home Assistant YAML. Validation results are in Release_9_Validation.md. Production acceptance is pending until the actual licensed builds and park tests below pass.

## Authority and scope

Firmware repositories own counting, RTC/year safety, SD persistence, detector behavior and MQTT implementation. Home Assistant owns operating dates/times, reset offsets, extension and detector/alarm setpoints. MSB-Show-Controls is the governing cross-system documentation repository; its write permission remains blocked, so this release carries the proposed contract in both firmware repositories and this private handoff. It has NOT been published to Show Controls.

Both private secrets.h files may define AP 1 through AP 6. All six are attempted; AP 6 must be a complete SSID/password macro pair. No private credentials are committed. The first five AP registrations remain unchanged.

No changes to GPIO assignments, detector state-machine polarity, official 2025 metrics, LOR production actions, radios, lasers or the existing traffic analytics package. No reconstruction of missing historical vehicles. Gate bag-sensor mode is NOT included: GPIO32 is currently Beam A and the bag sensor's input/wiring contract has not been supplied. Dual-beam Gate testing requires its second beam.

## One ownership table

| Setting | Authoritative HA helper | Firmware use / first-use fallback |
|---|---|---|
| Show start date | input_datetime.msb_show_start_date | Saved valid current-year date; Black Friday if none |
| Nominal open/close | input_datetime.msb_show_start_time / msb_show_end_time | 17:00 / 21:00 before saved configuration |
| Car reset offset | input_number.carcounter_start_offset_min | Existing −1 minute; HA controls it |
| Gate reset offset | input_number.gatecounter_start_offset_min | Existing +10 minutes; HA controls it |
| Car official closing allowance | input_number.carcounter_end_offset_min | Existing +10 minutes |
| Gate official closing allowance | **input_number.gatecounter_show_end_offset_min** | New HA helper, initialized once to +25 minutes; then restored/editable |
| Extension | input_number.show_end_extension_min | 0–30 minutes, bound to the date it was explicitly changed |
| Show end / down date | input_datetime.msb_show_end_date / msb_show_down_date | Saved dates; Dec 31 / Dec 24 before saved configuration |
| Alarm timeouts | input_number.enter_stuck_timeout_s / gate_stuck_timeout_s | HA seconds converted to firmware milliseconds |
| Detection thresholds | input_number.enter_cardetectms / exit_cardetectms | Applied to existing Car waitDuration / Gate carDetectMS semantics |
| Clock, recording year, folder, day number | Firmware RTC, validated NTP and saved high-water year | Cannot be set by retained calendar reset commands |
| Raw daily/hourly traffic | Firmware | Continues all day, all year, independent of official show windows |

The old Gate end-offset helper with value 0 does not become a midnight official-show cutoff. Raw recording remains all-day. It is not repurposed. The analytics cushion helpers remain untouched.

Accepted HA settings are stored on SD with readback verification, plus NVS fallback. Config messages never reset counters. The scheduler executes a reset once per date at HA's configured minute and saves that marker with the counts. Booting later does not trigger a catch-up reset that might erase recovered cars. If the MCU is off throughout the reset minute, it does not invent a reset on startup.

## Agreed windows

| HA extension | Car reset | Gate reset | Car official cutoff | Gate official cutoff |
|---|---|---|---|---|
| 0 | 16:59 | 17:10 | 21:10 | 21:25 |
| 15 | 16:59 | 17:10 | 21:25 | 21:40 |
| 30 | 16:59 | 17:10 | 21:40 | 21:55 |

These rows are the result of the current HA helper values, not fixed firmware reset commands. The cutoff instant is excluded. Before reset and after cutoff, vehicles remain in raw traffic. The 17:10 Gate reset excludes opening volunteers ahead of guests; 1.1 miles at 5 mph is about 13 minutes 12 seconds. The closing allowance avoids counting the later volunteer departure as official guest exits. Busy-night capacity and prior-year indicators are outside this change.

Set an extension in HA AND LOR before the current cutoff. Firmware refuses late extensions that would conceal an already missed interval, and refuses shortening an extension after nominal closing has begun. Before nominal close, settings can be tested and returned to 0. Extensions expire on the next date in both HA and firmware; restarting HA cannot carry last night's extension into tonight. To extend again on a later night, change the helper explicitly (0 then 15/30 if necessary).

## Metrics and durability

Both devices write ShowSummary.csv and ShowSummaryBuckets.csv in their current-year folder. Car's existing web pages read the added columns automatically. Gate also exposes /ShowSummaryBuckets.csv and /showSummaryBuckets.html using its existing viewer; no SD UI upload is required.

6PM, 7PM, 8PM and 9PM are consistently hour-ending labels. The old behavior that folded 21:00 traffic into 9PM is removed. DailyTotal includes all official counts through the configured cutoff, including the closing allowance. On normal nights, added 10PM and ExtensionMin fields are blank; on extended nights they report the late interval and extension. Therefore a normal night's DailyTotal can exceed the sum of the four displayed buckets. 10PM is an hourly bucket in ShowSummaryBuckets, cumulative in ShowSummary. It is not a promise to keep counting until 22:00.

New columns are appended; existing column positions and historical row values are preserved. Only the current-year CSV can be upgraded. Each replacement is verified and preserves the previous complete file. Retry/reboot updates the same date instead of appending duplicates. Unknown schemas or incomplete recovered official counts produce a warning and leave the metrics file intact. A mid-show upgrade from .5 cannot reconstruct official bins from raw all-day bins; install before the show and do not treat that partial upgrade day as a complete official summary.

Versioned count checkpoints migrate .5 daily/show/hourly totals and reset markers. Each vehicle checkpoints raw and official counts together. At date rollover, ClosedCounts-YYYYMMDD.bin preserves the closed day's exact checkpoint before its slots can be reused. An SD failure blocks durable saves and raises diagnostics; it cannot guarantee persistence of RAM-only vehicles through a power loss. Failed summary exports are retried that day; the closed-day binary archive preserves evidence if a CSV export remains incomplete.

The .9 checkpoint format is newer than .5. **Do not downgrade by simply flashing .5 after .9 has recorded traffic.** It cannot read the new checkpoint format. Preserve files and use a reviewed recovery procedure; never upload old totals over newer counts.

## Installation: one coordinated change, device by device

1. Before leaving home, pull both refactor/shared-firmware branches, run each verifier (Gate with --peer ../MSB-Car-Counter-2024), and build both using your existing PlatformIO environment and licensed ElegantOTAPro. Confirm each built source says modular-test.9. Do not upload a compile-contract/shim image. Keep the devices on .5 until both real builds succeed.
2. At the park, record both current counts, firmware versions, RTC local time and SD health. Create a normal HA backup. Run Backup-Traffic.ps1 from this package to collect both current-year SD file sets in one operation; it changes only the file manager's viewing directory. Inspect Backup-results.csv. Do not concurrently browse/change those directories. Missing Gate bucket CSV before the first .9 summary is expected; missing count checkpoints is not.
3. Upload the correct Car .9 firmware. Wait for reboot; confirm .9, /CC/current-year, calendarHealth OK and countPersistence OK. Compare the recovered counts with step 2. Stop if they differ unexpectedly; do not reset them. Repeat this single controlled upload/check for Gate, expecting /GC/current-year.
4. In HA, save current files OUTSIDE all merged include directories. Replace automations.yaml, templates.yaml and the five mqtt_configs files from this package. Add packages/traffic_firmware_settings.yaml. Do not replace configuration.yaml, input_number.yaml or packages/traffic_analytics.yaml, and do not duplicate UI-created helpers in YAML. No scripts.yaml changes are needed.
5. Run HA's configuration validation. If it fails, restore the prior YAML and stop. After a successful check, restart HA once to load the new helpers/package and automations. Confirm the new Gate official-end-offset helper is 25, and your existing reset-offset helpers remain −1 and +10. Keep manual-reset permission booleans off. No count correction is sent by startup/config synchronization.
6. Wait for State/operatingSettings and applied-minute topics on BOTH devices (startup/hello/helper changes and a five-minute retry trigger synchronization). Expect settingsHealth OK or OK_RESTORED. New configuration automations must be enabled; restored states of existing automations are not forcibly enabled by this package. Do not use Run Actions on a manual count-reset automation.

### Read-only expected MQTT evidence

Under msb/traffic/{CarCounter,GateCounter}:
- System/firmware: device-specific modular-test.9.
- System/seasonFolder: /CC/YYYY or /GC/YYYY, matching RTC year.
- System/calendarHealth and System/countPersistence: OK.
- System/settingsHealth: OK (or OK_RESTORED after reboot).
- State/resetMinute: Car1019, Gate1030 with the current HA offsets.
- State/countEndMinute: Car1270, Gate1285 without extension.
- State/extensionMinutes:0; State/showStartDate:2026-11-27 this year.
- Cars/hour/json: 24 current raw bins on both devices.
- System/hourlyExport: OK following an export; System/summaryExport: NOT_DUE before any summary, OK after successful summary.

HA command topics remain under Config; firmware acknowledgements/state remain under State. The atomic operatingSettings payload is versioned CSV for transport only; users edit the existing HA helpers. Legacy retained per-setting commands and count/calendar reset topics are inert. Manual count overrides use a separate non-retained, 60-second dated, replay-protected command generated only by the permission-gated HA actions.

## Morning field acceptance — no clock/date manipulation

1. Record a starting daily/raw-hour count. Send seven deliberate vehicles/events through the real detector at normal spacing. Confirm +7 on the device, current-year hourly CSV and HA. Gate needs both beams; do not infer a broken Gate detector from an absent second beam.
2. Reboot that device once. Confirm those seven increments survive exactly. Do one device at a time; keep traffic controlled for the test.
3. Repeat a small known count with MQTT unavailable, then with Wi-Fi unavailable. Confirm the loop continues counting and SD advances. Reboot offline; confirm recovery before reconnecting. On reconnect, the latest state must appear, without replaying older retained counts.
4. Change a reset offset temporarily in HA to a value whose reset minute is not occurring during this test. Verify State/resetMinute changes on the correct device and neither counter changes its count. Reboot, verify the accepted offset survives, then restore your original HA value and verify again.
5. Before nominal closing, change extension 0 → 15 → 30 → 0 in HA. Confirm the exact State/countEndMinute pairs from the table and that counts do not reset. Return to 0 and verify both acknowledgements before ending the test. No RTC/date manipulation is needed. Actual late-night bucket boundaries are additionally covered by the host tests; verify them on an isolated bench or the first genuinely extended night.
6. Observe the first actual evening reset/cutoff/summary after the configuration test. Confirm no duplicate summary rows after restart. Check the 2025 metrics files remain byte-identical to their backups.

Keep MQTT Explorer open for observations, but it is not the persistence mechanism. HA persistent notifications report storage/configuration faults; inspect and dismiss them after resolving the cause.

## Rollback / stop conditions

Stop before the next mutation for a version mismatch, wrong year/folder, unexpected count change, failed HA validation, persistence fault or rejected settings. Preserve SD files and the new test evidence. YAML can be restored from the private HA backup, but old command topics are intentionally inert on .9; that is a safe hold, not a functioning .5 rollback. Firmware rollback with new count records requires an explicit compatible migration. Do not clear retained broker topics, format the SD card or rewrite 2025 to make a warning disappear.
