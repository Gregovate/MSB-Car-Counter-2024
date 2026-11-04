# Modular extraction validation

Status: PASS for source-preservation checks and ESP32 compile/link contract checks. Not approved for upload.

| Check | Car | Gate |
|---|---|---|
| Baseline function-body token preservation | 56 functions PASS | 61 functions PASS |
| AP registration/setup position | PASS | PASS |
| GPIO/topic macros, state initialization order/values, enum values | PASS | PASS |
| Common files byte-identical across projects | 9 files PASS | 9 files PASS |
| ESP32 compile and link | PASS with OTA interface shim | PASS with OTA interface shim |
| Real licensed ElegantOTAPro build | Pending operator-local build | Pending operator-local build |
| Hardware/OTA/counting acceptance | Not performed | Not performed |

Compile configuration used Espressif32 platform6.9.0, Arduino ESP32 framework2.0.17 package3.20017.241212, Xtensa8.4.0 toolchain2021r2-patch5, board az-delivery-devkit-v4. Public libraries: SSD1306 2.5.9, GFX1.11.9, BusIO1.16.1, RTClib2.1.3, NTPClient3.2.1, PubSubClient2.8, ArduinoJson7.2.0, DHT1.4.6, Unified Sensor1.1.14, AsyncTCP3.3.2, ESPAsyncWebServer3.6.0.

The locally generated ElegantOTAPro interface stand-in contains no paid source and implements no OTA operation. It and the invalid test credentials/local build configuration are not part of this PR. No image produced using that stand-in is supplied as a deployable artifact. Normal platformio.ini and the real local licensed library remain the path for an actual build.

The checks establish that the structural extraction compiles/links and preserves the original decision code. They do not prove timing, count accuracy, physical GPIO wiring, power-loss recovery, real OTA behavior or correction of assessment findings. Original function comments were preserved inside moved functions; global inline notes were retained in RuntimeState.cpp, and source history is archived in docs/Firmware_History.md.

User-visible test identities: Car25.12.03.2-modular-test.1 and Gate25.12.28.0-modular-test.1. Neither is a production release.
