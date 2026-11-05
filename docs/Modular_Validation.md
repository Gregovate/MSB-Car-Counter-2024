## Published checksum correction — 2026-11-04

The first `.3` shared-file manifest incorrectly hashed nine unchanged files from temporary copies containing an extra final newline. Clean Git checkouts reproduced `FAIL: shared file changed: src/services/MqttTransport.cpp` in both repositories. This was a manifest-generation error, not an operator edit or Windows CRLF problem.

Corrected only the nine expected digests to match the committed LF-normalized bytes. The verifier remains strict; no check was disabled and no source files were padded to match the incorrect hashes. Clean Git checkouts now pass the source verifier, all 17 shared-file comparisons against the peer, and all 11 tests per project (including Windows encoding/CRLF coverage). The previous temporary-workspace validation was insufficient to validate the published manifest and is superseded by this checkout-based result. Firmware source and `.3` identity are unchanged; no new compile or device upload is involved.

Future release verification must run from the exact published Git checkout, not only a reconstructed working copy.

> Update 2026-11-04: `.3` deliberately changes calendar/storage behavior. See [Recording year protection](Recording_Year_Protection.md) for the current rules, validation and remaining work. The structural-refactor evidence below is historical.

# Modular extraction validation

Status: PASS for source-preservation checks and ESP32 compile/link contract checks. Not approved for upload.

| Check | Car | Gate |
|---|---|---|
| Baseline function-body token preservation | 56 functions PASS | 61 functions PASS |
| AP registration/setup position | PASS | PASS |
| GPIO/topic macros, state initialization order/values, enum values | PASS | PASS |
| Common files byte-identical across projects | 9 files PASS | 9 files PASS |
| ESP32 compile and link | PASS with OTA interface shim | PASS with OTA interface shim |
| Real licensed ElegantOTAPro build | PASS: operator build at6fe838b, ElegantOTAPro3.1.4 | PASS: operator modular build log, ElegantOTAPro3.1.4 |
| Hardware/OTA/counting acceptance | Not performed | Not performed |

Compile configuration used Espressif32 platform6.9.0, Arduino ESP32 framework2.0.17 package3.20017.241212, Xtensa8.4.0 toolchain2021r2-patch5, board az-delivery-devkit-v4. Public libraries: SSD1306 2.5.9, GFX1.11.9, BusIO1.16.1, RTClib2.1.3, NTPClient3.2.1, PubSubClient2.8, ArduinoJson7.2.0, DHT1.4.6, Unified Sensor1.1.14, AsyncTCP3.3.2, ESPAsyncWebServer3.6.0.

The locally generated ElegantOTAPro interface stand-in contains no paid source and implements no OTA operation. It and the invalid test credentials/local build configuration are not part of this PR. No image produced using that stand-in is supplied as a deployable artifact. Normal platformio.ini and the real local licensed library remain the path for an actual build.

The checks establish that the structural extraction compiles/links and preserves the original decision code. They do not prove timing, count accuracy, physical GPIO wiring, power-loss recovery, real OTA behavior or correction of assessment findings. Original function comments were preserved inside moved functions; global inline notes were retained in RuntimeState.cpp, and source history is archived in docs/Firmware_History.md.

User-visible test identities: Car25.12.03.2-modular-test.1 and Gate25.12.28.0-modular-test.1. Neither is a production release.

## Operator-local Car build evidence

The operator supplied the complete successful PlatformIO build log after pulling6fe838b. Actual environment: Espressif32 6.9.0; Arduino ESP32 framework3.20017.241212+sha.dcc1105b; Xtensa8.4.0+2021r2-patch5; ElegantOTAPro3.1.4 (private local library); AsyncTCP3.3.1; ESPAsyncWebServer3.4.5; SSD1306 2.5.13; GFX1.11.11; RTClib2.1.4; NTPClient3.2.1; PubSubClient2.8.0; ArduinoJson7.2.1; DHT1.4.6; Unified Sensor1.1.14.

Compile and link SUCCESS in24.17 seconds. RAM50504/532480 bytes (9.5%); flash1040925/1310720 bytes (79.4%). Three warnings appear in the third-party Arduino-DS3231 1.1.0+sha.4582ed6 source: sequence-point/month assignment, uint32_t printf format, and possibly uninitialized month. No application compile/link errors appear. Both firmwares instantiate RTC_DS3231 from RTClib; the separately declared Arduino-DS3231 dependency can be evaluated for removal in a later dependency cleanup, without changing it during this structural check.

This closes the Car real-library compile gate only. No paid library source was received or published. No upload or hardware acceptance is established by this log. Gate real-library build is recorded below.

## Operator-local Gate build evidence

The operator supplied a successful Gate modular build log, including compilation of main.cpp and the extracted services (no Car ArchEffects.cpp), using the real local ElegantOTAPro3.1.4. Toolchain and reported library versions match the operator-local Car environment above. Compile and link SUCCESS in19.91 seconds; RAM50488/532480 bytes (9.5%); flash1024365/1310720 bytes (78.2%). The same three Arduino-DS3231 third-party warnings are present; no application compile/link errors appear.

The log does not print a Git commit or firmware version; its successful compilation of modular Gate services is direct evidence, while exact source/binary association still requires local revision verification and a binary hash. No firmware upload or runtime/hardware validation is shown. No licensed source was received or published.

## Manual release workflow update (.2)

The active changelog and single editable FWVersion are restored to the top of src/main.cpp. FirmwareVersion.h provides only the shared extern declaration. Every change is recorded newest-first in main.cpp, including this workflow update; original historical entries remain unchanged in docs/Firmware_History.md.

Three portability/release-workflow regression tests pass per project: Windows legacy encoding with CRLF, manual version/changelog edits accepted, and changed setupServer executable text rejected. All original function bodies, state defaults, GPIOs/topics and enums remain preserved. Ten shared files now include the new declaration-only version header. This .2 change supersedes the prior DeviceConfig.h version-edit location. Previous operator real-library build evidence applies to .1, not to the new .2 identity; a new local licensed-library build is still required before testing.

The workflow revision also passed an ESP32 compile/link check with public dependencies and a compile-only OTA API stand-in. This does not replace a local build with the licensed ElegantOTAPro library or hardware testing.



## Count recovery follow-up — 2026-11-04

The `.4` source adds shared dated per-vehicle checkpoints and protected hourly CSV replacement. See [Count persistence](Count_Persistence.md) for evidence, failure handling, remaining limits, and the controlled acceptance procedure. Neither device has been uploaded with this revision.


## `.5` zero-baseline migration regression — 2026-11-05
Exact field fixture fails on `.4` and passes after the shared correction. Both full 13-test suites and all 23 shared-file checks pass; tested source retains prior malformed/nonzero/clock/archive protections. The `.5` ESP32 compile check could not run in this session because PlatformIO is unavailable (`No module named platformio`). Prior `.4` compile evidence remains valid for `.4` only. Licensed `.5` builds and live count/reboot acceptance remain pending.

## Startup telemetry correction — 2026-11-05

Both devices now report `.5` installed. Car boot 04:59:16: `/CC/2026`, calendarHealth/countPersistence/hourlyExport all OK, daily/show 0/0. Car licensed `.5` build passed in 11.87 s; OTA failed MD5 twice, then succeeded on the third reported attempt. The reason for those MD5 failures is not established.

Gate boot 05:05:16: `/GC/2026`, calendarHealth/countPersistence OK, exit/in-park 0/0, accepted State/showStartDate=2026-11-27. Gate rejected the stale 2025-11-28 command as intended. Its OTA identity/success and heartbeat confirm `.5` installation; no separate Gate build log/duration was supplied. Nonzero counting/reboot acceptance and offline operation remain pending.

Gate `System/hourlyExport` was absent. Source audit: boot restores/exports before MQTT connects. Gate queues these messages but has no call to `publishQueuedMessages`; the current export status was only sent at the file operation. Thus a successful startup export could have no MQTT status. This finding alone does not prove that the CSV export succeeded; inspect its latest status/file after correction.

Shared `.6` latches the latest hourly-export result and repeats it retained through `publishCountHealth` on each heartbeat in both counters. It also reports previously silent read/skip cases. `NOT_ATTEMPTED` means no result yet; failure remains visible until a subsequent export result replaces it. Heartbeat publication does not rewrite SD or change counts. The existing offline event queue is not replayed: replaying historical retained counts into HA requires a separate queue/state reconciliation design.

Regression tests discard the initial publication, then verify heartbeat publishes retained OK without changing files; repeat after a failed write and require the failure, then verify successful export replaces the latched error. Both 13-test suites pass; peer verification checks 23 shared files and 90 Car / 95 Gate function bodies. ESP32 `.6` build checks were not available here (PlatformIO absent). Licensed `.6` builds, uploads and runtime verification are pending; both current devices remain `.5`.

Next acceptance: build/install the Gate reporting revision using the existing controlled procedure, then observe `System/hourlyExport` after a heartbeat. On any failure inspect `/GC/2026` evidence, without a count reset. Verify current accepted date remains 2026-11-27. Preserve nonzero-count/reboot testing as a separate acceptance step.
