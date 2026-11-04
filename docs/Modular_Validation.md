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
