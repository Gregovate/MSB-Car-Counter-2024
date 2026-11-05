> Current update, 2026-11-05: Car `.4` licensed build passed (29.77 s) and OTA identity confirms installation. Its first migration stopped with `RESTORE_FAILED_KEEP_FILES`. The downloaded 2026 files prove a zero daily baseline with mismatched older hourly/day records. Shared `.5` corrects that narrow migration case and adds the exact regression fixture. No `.5` upload yet; Gate last observed `.2`. See [Count persistence](Count_Persistence.md). Capture Car `System/unrestoredDailyCount` before any further firmware reboot.

> Current source update, 2026-11-04: `.4` adds shared per-vehicle count checkpoints/recovery and protected hourly replacements. [Count persistence and acceptance procedure](Count_Persistence.md) supersedes earlier persistence limitations. Both `.3` licensed local builds passed, but neither `.3` nor `.4` has been reported uploaded. Devices remain `.2`; current-year-only migration does not reconstruct today's missing counts. `.4` licensed build/hardware tests pending. Earlier entries below are historical.

> Later update, 2026-11-04: Greg subsequently flashed both `.2` devices and supplied runtime evidence. Car was writing into /CC/2025 despite /CC/2026 telemetry. The new `.3` source implements calendar-year protection and Black Friday fallback; see [Recording year protection](Recording_Year_Protection.md). This `.3` has NOT been uploaded. The original handoff below is retained as historical evidence.

# Firmware testing checkpoint — 2026-11-04

## Scope and branch
Both repositories use refactor/shared-firmware. Implementation PRs: Car #3; Gate #4. Production main branches are unchanged by this checkpoint. Structural refactor and manual release workflow are complete; hardware acceptance is pending.

## Build evidence reported by Greg
- Car: pulled workflow revision 4d4c5fb; local licensed ElegantOTA build SUCCESS, 7.00 seconds. RAM 50,504 / 532,480 bytes; flash 1,040,913 / 1,310,720 bytes.
- Gate: local build SUCCESS, 20.02 seconds, ESP32 firmware image created. Full pull output/commit and size were not supplied for this latest Gate build; verify checkout/version before installation.
- No upload to either device has been reported in this session.
- Car FWVersion: 25.12.03.2-modular-test.2.
- Gate FWVersion: 25.12.28.0-modular-test.2.
- Prior preservation checks passed: Car 56 and Gate 61 original function bodies; AP registration, GPIO/MQTT definitions, state initialization and enums. Ten shared files match. Three portability/workflow tests passed per repository. This is source/build evidence, not hardware validation.

## Preserved workflow and implementation
FWVersion and the active newest-first changelog are at the top of src/main.cpp. Record every firmware change there and manually advance the identifier before compiling. include/traffic/FirmwareVersion.h declares the single value for all modules. Original history remains in docs/Firmware_History.md.
Services separate network, MQTT transport/reporting, web/OTA, temperature/environment, clock, storage, detection and display. Car additionally has ArchEffects. Shared files are mirrored and verified; no central shared library yet.
Existing variable names, GPIO assignments, constants, state defaults, MQTT topics/payload fields and counting behavior were preserved. No HA configuration change is required by the structural refactor. Existing separate web assets and OTA screens remain.
Car beams A=33/B=32. Gate beams A=32/B=33; Gate still uses legacy magSensorPin for Beam A. Pin/name standardization has NOT occurred.
ElegantOTAPro is licensed, remains local, and must never be published. No secrets, private library source or test stand-in firmware should be deployed.

## Hardware and operational constraints
Car is seasonal, with larger physical display and red/green/dancing arch effects. Gate has a small OLED and no arch outputs. Both have stuck-beam alarms.
Gate's intended operation is show-season dual beam and off-season magnetic on the same ESP32. A new runtime mode switch has NOT been implemented.
Gate network is down due to the unrepaired park cable. Greg must visit for installation/testing there. Car is remotely reachable, but its physical display/arch behavior cannot be observed remotely.
Keep production firmware and data available for rollback. Do not upload or reset counts without an explicit testing decision from Greg.

## Outstanding work
- Hardware acceptance: reboot/persistence, detector sequences and counts, stuck-beam alarm/recovery, Wi-Fi/MQTT reconnect, HA reporting, time and daily rollover, SD logging, web pages, OTA, physical displays, Car arches.
- 2026 season/year storage behavior remains a separate required change, NOT implemented by this refactor. Preserve all 2025 data unchanged. Verify existing paths before any real counting/rollover test; avoid writing test counts into historical files. January season handling also needs design/validation.
- Web pages should retain clearly distinct Car/Gate titles/colors. Further UI unification is pending.
- Nightly reporting replacement and live matrix/LOR display are future work.

## Start of next testing thread
Read this checkpoint plus docs/Modular_Firmware.md and docs/Modular_Validation.md in both refactor/shared-firmware branches. Start with read-only checkout/version and storage-path verification, then agree on an isolated hardware test and rollback procedure. Give Greg one terminal step at a time. Do not assume a successful build proves device behavior or that a device has been updated.

