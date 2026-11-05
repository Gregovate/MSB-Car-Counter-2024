## 25.12.03.2-modular-test.7 — 2026-11-05
- Share command-to-State reporting for detector timing; firmware no longer overwrites HA-owned retained tuning commands during startup.
- Report actual settings on heartbeat and after supported command application; skip disconnected snapshots to avoid stale state replay.
- Keep existing command subscriptions, detector behavior, defaults, reset timing and count storage unchanged. Car beam-duration command remains disabled and is documented explicitly.
- See [Configuration ownership](Configuration_Ownership.md). 14 host tests PASS per device; 24 shared files match. Licensed builds and hardware verification pending.

## 25.12.03.2-modular-test.6 — 2026-11-05
- Latch and repeat current hourly-export health retained on heartbeat in both devices, so pre-connection startup messages do not disappear from monitoring.
- Explicitly report skipped/checkpoint/SD/read failures. Do not replay old MQTT count queues or change counting/storage formats.
- Add tests for lost initial publication, retained failure visibility, recovery to OK, and no SD writes from status publication.
- Both devices confirmed on `.5`; `.6` licensed build/hardware verification pending.

## 25.12.03.2-modular-test.5 — 2026-11-05
- Fix `.4` migration rejection when a verified zero daily total/day marker is newer than valid older hourly rows; keep all older rows unchanged and do not derive today's count from them.
- Exact field regression: EnterTotal=0, empty ShowTotal, day=4, October 19/21 hourly rows, RTC November 5. `.4` fails this fixture; `.5` restores zero and preserves a subsequent vehicle across reboot. Inconsistent nonzero totals still stop recovery.
- Manual version/changelog updated. Same shared fix in both counters; licensed builds and `.5` hardware acceptance pending.

## 25.12.03.2-modular-test.4 — 2026-11-04
- Share immediate per-vehicle dated SD checkpoints and reboot recovery across both counters.
- Verify alternating copies; retry failed saves independently of MQTT; report retained persistence faults.
- Preserve counts across same-day/reset-minute reboots; use full-date rollover and stop uncertain recovery from overwriting files or HA counts.
- Verify hourly CSV replacements with previous-file recovery; preserve all historical-year write guards.
- Add real event/reboot and SD failure tests. Licensed build and hardware acceptance pending; see [Count persistence](Count_Persistence.md).

## Verification manifest correction — 2026-11-04

- Correct nine shared-file checksums generated from temporary copies with an extra trailing newline.
- Reproduce and verify the correction in clean Git checkouts of both counters: source/peer checks and 11 tests per project PASS.
- No firmware behavior change; manual firmware versions remain `.3`.

## 25.12.03.2-modular-test.3 — 2026-11-04

- Shared persistent calendar-year routing and historical-folder write protection.
- Black Friday fallback; validated current-year HA override saved to NVS and two verified SD copies.
- Shared days-running calculation (zero before opening), current-year show gating, and separate accepted-date telemetry.
- Runtime year rollover and network-clock retry without moving the recording year backward.
- Web upload/delete/rename protection; per-request upload handles.
- MQTT connects after SD restoration; Car also restores show total on a new-day reboot.
- Native regression checks and ESP32 compile/link checks pass; licensed build/hardware acceptance pending.
- Scope and remaining reliability work: [Recording year protection](Recording_Year_Protection.md).

## Changelog

### [1.3.0] - 2024-12-19
#### Added
- Published hourly average temperatures via MQTT.
- Improved temperature and humidity logging logic for accurate summaries.
- Enhanced state machine logic for vehicle detection reliability.

#### Fixed
- Resolved issue with average show temperature always being zero.
- Corrected redundant declaration of `hourlyTemp` array in `averageHourlyTemp()` function.

#### Changed
- Refactored `saveDailyShowSummary()` for better handling of hourly temperature calculations.
- Streamlined MQTT topic naming for temperature and hourly data.

### [1.2.0] - 2024-12-10
#### Added
- Integrated DHT22 sensor for temperature and humidity monitoring.
- Hourly temperature averages calculated and stored for summaries.
- SD card functionality to log vehicle counts and environmental data.

#### Fixed
- Resolved MQTT connection stability issues with improved reconnect logic.
- Debugged data recovery process during reboots to prevent count loss.

#### Changed
- Updated MQTT topics to support environmental data.
- Improved logging for better debugging and monitoring.

### [1.1.0] - 2024-12-01
#### Added
- Implemented ElegantOTA for firmware updates.
- Introduced WiFiMulti for seamless multi-network connection support.
- Added OLED display support for real-time monitoring of counts and temperature.

#### Fixed
- Addressed sensor bounce issues during detection in snowy conditions.
- Resolved SD card initialization failure for specific configurations.

#### Changed
- Optimized state machine logic for car detection.

### [1.0.0] - 2024-11-15
#### Added
- Initial implementation of car counter using ESP32.
- Vehicle detection with IR beam sensors.
- MQTT integration for remote monitoring of vehicle counts.
- SD card support for local data logging.
- Time synchronization using NTP and RTC.
- Basic OLED display support.

---

## Future Enhancements
- Add a web-based dashboard for real-time data visualization.
- Integration of additional environmental sensors for advanced analytics.
- Enhanced power efficiency for long-term, battery-powered deployments.

---

This changelog follows semantic versioning. For further details, please contact:
**Greg Liebig**  
Email: `greg@engrinnovations.com`

