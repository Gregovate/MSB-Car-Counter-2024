# Configuration ownership and remaining refactor work

Source audit: 2026-11-05, modular-test.7. Governing operational design belongs to MSB-Show-Controls. This document records actual firmware behavior and gaps; it does not introduce traffic indicators or redo the 2025 traffic analysis.

## Command and state separation

HA owns retained commands. Firmware reports the applied value on a separate retained State topic. In .6 and earlier, startup publishTimingConfig wrote defaults to the command topics below. That could replace HA's retained configuration before commands were processed. .7 removes that collision in both devices using one shared publishConfigState implementation.

All paths below begin with msb/traffic/. Values are milliseconds.

| Device | HA command suffix | Applied state suffix | Actual command behavior / startup fallback |
| --- | --- | --- | --- |
| CarCounter | Config/carCounterTimeout | State/carCounterTimeout | Accepted; existing clamp 5000–600000; startup 60000 |
| CarCounter | Config/beamWaitDuration | State/beamWaitDuration | Handler is disabled; startup 950 remains firmware-controlled |
| GateCounter | Config/gateCounterTimeout | State/gateCounterTimeout | Accepted using existing atoi conversion; startup 60000 |
| GateCounter | Config/carDetectMS | State/carDetectMS | Accepted using existing atoi conversion; startup 1000 |

Firmware repeats applied timing state on each heartbeat and acknowledges supported timing commands through State. Disconnected timing snapshots are skipped, not queued. This prevents pre-command defaults from later replaying as current applied settings. Firmware never acknowledges the disabled Car beam command as applied; its State continues to report its actual 950 default.

Existing command topic names and subscriptions are unchanged. Existing HA command publishing continues to work. HA views requiring actual applied settings should consume State; Config describes requested values and may differ. No HA files were changed in this release. Existing stale retained commands are neither deleted nor corrected automatically.

This release does not implement SD persistence of these four tuning settings. Offline startup still uses the listed firmware defaults. Strict payload validation and verified persistent configuration are required next; do not describe the current tuning settings as an implemented SD fallback.

## Scheduling and configuration audit

| Area | Car Counter | Gate Counter | Remaining work |
| --- | --- | --- | --- |
| Recording year / date | Shared validated RTC/network calendar, current-year folder protection | Same shared logic | Hardware rollover/offline acceptance |
| Show start date | Validated current-year HA command; persisted date override and Black Friday fallback | Same shared date logic; listens to Car date command | Reconcile HA's stale retained 2025 command; verify State |
| Show start/end minutes | HA Config/showStartTime and showEndTime; existing permissive hh:mm parser; RAM only | Constants 17:10 and 21:20 | Shared validated persistent configuration and HA mapping |
| Scheduled daily reset | showStartMin minus 1 minute | Hard-coded 17:08 despite 17:10 comment | Honor agreed HA show-relative offsets consistently |
| Show summary trigger | showEndMin | Hard-coded 21:20 | Reconcile intended end/stop offsets and persist summary completion |
| Hourly data | All-day hourly array survives show daily reset | Same | Preserve park traffic while defining show-window summaries separately |
| Days running | Shared date-derived value; legacy manual reset handler still exists | Shared date-derived value; legacy manual reset handler still exists | Resolve manual overrides versus derived-state ownership |
| Connectivity | Blocking Wi-Fi setup/fallback; synchronous reconnection | Blocking Wi-Fi setup/fallback; synchronous reconnection | Counting and RTC/SD initialization must not depend on network availability |
| MQTT backlog | Legacy queue flush at reconnect | Startup queue not drained | Reconcile current state without replaying stale count values |

The gate reset is intentionally later than the entrance reset: operations volunteers leave through the exit after opening gates, before the first show visitors arrive. The screenshot's +10-minute gate offset is not implemented by the hard-coded 17:08 condition. This .7 change does not silently alter either device's timing. The governing Show Controls documentation draft could not be published because the GitHub integration rejected writes (403); do not treat it as a published authority.

## Acceptance procedure for this release

Use the firmware-only update procedure in Count_Persistence.md. Do not reset counts or change detector tuning just to test telemetry.

1. Build the appropriate .7 firmware after the peer verifier passes. Preserve current device counts and health before an agreed update.
2. Observe the four existing Config values and corresponding State values read-only. After startup/heartbeat, firmware must not replace Config with its defaults.
3. Supported HA commands should appear as applied State after processing; a difference for the disabled Car beam command is expected and must remain visible.
4. Verify current-year folder, countPersistence, hourlyExport and reboot count continuity. Licensed builds, OTA and hardware acceptance remain pending for .7.

Host tests compile the actual shared publisher and device-specific timing publisher against a retained-command broker model. They verify that startup preserves HA commands, applied state reports real values, offline snapshots do not publish, and a later connected snapshot reports current values. These tests do not prove real MQTT delivery or physical sensor operation.

See [Three-system MQTT contract](MQTT_Contract.md) for the .8 reporting corrections and explicitly pending operational configuration work.
