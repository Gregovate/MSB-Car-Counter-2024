> Update 2026-11-04: `.3` deliberately changes calendar/storage behavior. See [Recording year protection](Recording_Year_Protection.md) for the current rules, validation and remaining work. The structural-refactor evidence below is historical.

# Modular firmware — first implementation

Status: DEVELOPMENT / TEST ONLY. No device upload is part of this change.

This change implements matching module boundaries in the Car and Gate firmware. It moves existing code into separately compiled `.cpp` files with guarded declaration headers. It does not just include sections of the old main.cpp into another file. Existing `include/gatecounter.h`, `include/sdfilesystem.h`, `include/WebPage.h` and `src/mqtt_common.h` remain untouched as historical/reference work.

## File ownership

| Files | Responsibility |
|---|---|
| `src/main.cpp` | Setup sequence and loop scheduling, still in the original order |
| `include/traffic/DeviceConfig.h` | Existing device identity, GPIOs and logical display geometry; version declaration imported from FirmwareVersion.h |
| `include/traffic/MqttTopics.h` | Exact existing topic literals; spelling/case/slashes preserved |
| `include/traffic/Firmware.h` | Composition header connecting service declarations; no object definitions |
| `include/traffic/RuntimeState.h`, `src/services/RuntimeState.cpp` | Transitional extern declarations and exactly one definition of each original global object/value |
| `MqttTransport.cpp`, `MqttService.cpp` | Common queue/diagnostic helpers; device-specific publish wrapper, telemetry and HA callbacks |
| `NetworkService.cpp` | Wi-Fi setup/recovery and MQTT connection; the only translation unit including private `secrets.h` |
| `WebCommon.cpp`, `WebService.cpp` | Common helpers/OTA callbacks; each device's existing HTTP routes, SD UI and ElegantOTA setup |
| `TemperatureSensor.cpp`, `EnvironmentService.cpp` | Common DHT read/warmup/error behavior; device-specific hourly-average publishing |
| `ClockCommon.cpp`, `ClockService.cpp` | Common RTC helpers; device-specific day/show scheduling and resets |
| `StorageService.cpp` | SD mount/health, paths, restore, checkpoints and summaries |
| `VehicleDetector.cpp` | Original detection states, stuck-beam alarm behavior and count event handler |
| `DisplayService.cpp` | Each device's local display layout |
| `ArchEffects.cpp` | Car-only arch dance; Gate has no effect outputs |
| `docs/Firmware_History.md` | Preserved source introduction/change history; earlier source remains in Git |

Each service has a corresponding header under `include/traffic`. Shared declarations do not allocate a second RTC, display, SD File, network connection, MQTT client or queue. The original initialization order is retained in RuntimeState.cpp. Credentials are included once; this avoids duplicate definitions when a local secrets.h contains configuration objects rather than macros.

## What is shared today

The common transport, DHT reader, RTC helpers, OTA/web helpers and shared interface files are byte-identical in both repositories. `tests/shared_files.json` records their hashes. Run the verifier with `--peer` to compare the two local projects. This is an explicitly checked mirror while both applications remain in separate repositories; it is not a centrally installed dependency. Do not edit one copy without updating/verifying the counterpart. A pinned common library can replace this mirror after the module interfaces are proven in hardware.

Device-specific implementations intentionally retain different show windows, topics, GPIO polarity/pull modes, display layouts, checkpoints, detector timing and arch behavior. Similar variable names are not renamed in a structural extraction. Legacy `magSensorPin` still means Beam A on the current Gate show firmware.

## GPIO and display baseline

| Function | Car name / GPIO | Gate show name / GPIO |
|---|---|---|
| Beam A | `firstBeamPin` /33 | `magSensorPin` /32 |
| Beam B | `secondBeamPin` /32 | `beamSensorPin` /33 |
| Red effect arch | `redArchPin` /25 | None |
| Green effect arch | `greenArchPin` /26 | None |
| DHT22 | `DHTPIN` /4 | `DHTPIN` /4 |
| SD chip select | `PIN_SPI_CS` /5 | `PIN_SPI_CS` /5 |

Wire/SPI bus pins are inherited from the existing ESP32 board variant; there is no new remapping. Car beam inputs remain INPUT_PULLDOWN; Gate beam inputs remain INPUT. Both sources configure128x64 SSD1306 logical geometry at0x3C. Operator reports a physically larger Car screen and a tiny Gate OLED; identical logical dimensions do not establish identical physical displays.

The discussed standardized Beam A33/Beam B32 layout plus separate Gate magnetic input is a future wiring/configuration change, not implemented here. A Gate runtime detector-mode selector is also a later feature. The historical magnetic+beam implementation is preserved in Git, not newly enabled by this extraction.

## State names and constants

| Existing names | Current meaning / treatment |
|---|---|
| `totalDailyCars`, `totalShowCars`, `hourlyCount` | Same storage types/names in both devices; entry versus exit semantics remain device-specific |
| `carCounterCars`, `inParkCars`, `lastcarCounterCars` | Gate-only received entry count, derived occupancy and last received entry count |
| `firstBeamPin` vs `magSensorPin` | Both are first-beam roles in the show implementations, but GPIOs differ; aliases not silently swapped |
| `firstBeamTripTime` vs `beamATripTime_ms` | Start of detector event; keep names for traceable move |
| `bothBeamsHighTime` vs `bothBeamsBroken_ms` | Latched qualification-state start; not proof of continuous physical overlap |
| `lastCarDetectedMillis` vs `lastCarDetected_ms` | Last completed count's monotonic timestamp |
| `stableFirstBeamState`/`stableSecondBeamState` vs `stableRawA`/`stableRawB` | Existing debounce state, including differing boot initialization |
| `carCounterTimeout` vs `gateCounterTimeout` | Device-specific stuck-beam alarm threshold; each default60000ms; command validation differs |
| `waitDuration` vs `carDetectMS` | Not equivalent: Car950ms A-start qualification versus Gate1000ms final-state duration; not consolidated |
| `minActivationDuration` | Car100ms; Gate150ms; existing state transitions retained |
| `maxABFollow_ms` | Gate900ms diagnostic-only interval; no timeout reset reintroduced |
| `debounceDelay` | Both50ms; comparison operator/order retained |
| `showStartMin`/`showEndMin` | Car mutable17:00–21:10 defaults; Gate fixed17:10–21:20 |
| `line1`…`line7`, `display` | Original OLED object and layout values; Car local readout remains useful independently of MQTT |

Both systems retain MQTT stuck-beam alarms, including the snow-blockage use case. Gate has no additional physical effect outputs. Operator's intended Car visuals are green on a count, red idle, dance after inactivity. The current executable detector/output ordering is preserved for this extraction; testing must reconcile any mismatch with that intent before a separate effect-state correction. No timing optimization or field reliability improvement is claimed from file moves alone.

## Validation and limitations

`python tools/verify_modular.py` checks every moved baseline function body at executable-token level, missing/duplicate functions, unchanged AP registration at the same setup position, and mirrored shared-file hashes. String literals are retained in comparisons. Optional `--peer PATH` compares actual common file bytes against the other project. These checks are meaningful for a large structural move, but are not hardware acceptance or proof of counting correctness.

Cross-file state remains a transitional global-state interface rather than fully encapsulated service objects. The reviewed defects (network blocking, queue drain gaps, recovery, checkpoint outcomes and report idempotency) remain intentionally visible for separate changes. Do not mistake preservation checks for fixes to those defects.

ElegantOTAPro is paid, proprietary software. Its source must remain local and must never be committed, uploaded to GitHub or bundled in a public validation artifact. Keep it in the ignored local `lib/ElegantOTAPro-<version>/` directory in each project; do not replace it with a public OTA library. A real release build requires the existing private secrets.h and the licensed ElegantOTAPro library installed locally. The repository did not contain that licensed library. Compile-only tests using an OTA API shim cannot validate OTA operation and must not be uploaded. No binary from such a test is a release artifact.

## Local workflow

1. Pull the development branch with a clean/preserved working tree. Keep secrets.h and the existing local ElegantOTAPro installation.
2. Run `python tools/verify_modular.py` (or `py tools/verify_modular.py` on Windows).
3. Build the ordinary PlatformIO environment with the real local libraries. Stop on compiler errors; retain full build output.
4. Review the module diff and build evidence before any hardware test. No GPIO rewiring is required for this extraction.
5. Before any future upload, use the governing operator runbook, known-good firmware/UI bundle and count-data backup. This document does not authorize an upload.

## Manual release workflow

The active changelog and the only editable firmware version are at the top of `src/main.cpp`, matching Greg's established workflow. Before compiling, record every change in that newest-first comment block and manually update `const char FWVersion[] = "...";` immediately below it. Keep detailed older history in `docs/Firmware_History.md`; do not rewrite it.

`include/traffic/FirmwareVersion.h` declares the version for all separately compiled services; it contains no duplicate value. MQTT, ElegantOTA and diagnostics therefore use the same manually assigned identifier. `update_version.py` reads that value in UTF-8; it changes it only when explicitly called with `--set`. It never assigns a version from old changelog comments.

The preservation check accepts a deliberate valid version change and comment-only changelog additions while continuing to reject altered function bodies, GPIOs, topics and initial state. The .2 workflow update changes release metadata and tools, not detector behavior. Earlier real-library build evidence applies to .1; .2 still needs a new local licensed-library build before any hardware test. UI files and CSV formats remain separate artifacts.



## Count recovery follow-up — 2026-11-04

The `.4` source adds shared dated per-vehicle checkpoints and protected hourly CSV replacement. See [Count persistence](Count_Persistence.md) for evidence, failure handling, remaining limits, and the controlled acceptance procedure. Neither device has been uploaded with this revision.
