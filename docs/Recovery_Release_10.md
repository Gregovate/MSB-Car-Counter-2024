# Release .10 — recovery after RTC time loss

Field failure, November 5, 2026: Car displayed 29 at 09:31:58, rebooted with January 1, 2000 and displayed zero, then recovered network time at 09:35:14 without restoring counts. Uploaded checkpoints were valid: A sequence 31/daily 29 and B sequence 30/daily 28. Saved data survived; restoration did not retry.

Both firmwares now retry the clock-dependent restoration after a valid current-year RTC date becomes available. Recovery is attempted at most once a second and stops on a checkpoint/SD integrity failure. It does not blindly retry corrupted evidence. Verified counts are loaded before web exports are repaired. Subsequent service calls cannot replace live counts with the startup snapshot.

While count recovery is blocked, the display says COUNT RECOVERY, not zero. Real detector events are committed separately in ESP32 NVS and reported as System/unassignedRecoveryCount and System/pendingCountHealth. They are deliberately NOT assigned to a guessed calendar date/hour or added to official show totals. They survive another reboot. This is a cumulative unresolved counter; no automatic reset or reconciliation is provided. NVS save failure is reported; durability is not guaranteed when storage itself fails.

The existing System/unrestoredDailyCount now publishes zero after successful restoration to clear stale retained fault telemetry. The one-valid-copy warning is based on the copies AFTER migration/save; an unsuccessful save retains its write failure instead of being masked by a recovery warning.

Both devices publish System/rtcHealth and System/rtcTimeLossObserved. The oscillator-stop flag is captured before NTP clears it. TIME_RECOVERED_CHECK_RTC_BACKUP remains for the current boot even after valid time returns; a subsequent clean boot clears the boot-scoped observation. This is not battery voltage or proof of a dead battery. A new/first-use RTC can also set the flag. DS3231 reference: https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf

Existing HA schedules, six-AP support, GPIO, production actions and prior-year protection are unchanged. No HA settings reinstallation is necessary. Optional HA diagnostics are provided in one additional package file; do not replace automations.yaml/templates.yaml again.

## Validation and field acceptance

Both 20-test host suites include the actual 29-car snapshot values, invalid clock, late recovery, repeated calls, second reboot, corrupt-checkpoint protection, durable unassigned events and real detector-event entry paths. RTC tests verify that time-loss warning survives clock recovery. Shared service files remain identical. ESP32 compile checks use the existing disposable ElegantOTAPro API substitute: never install those binaries. Build the licensed firmware locally.

At the park, confirm .10, correct time and the existing saved total before testing. With no new traffic, power-cycle once with the original RTC. If time starts invalid, the display must indicate recovery pending; after time returns, saved counts must return without manual resets. Compare checkpoints and System/countPersistence. New detections during the invalid-time interval appear separately in unassignedRecoveryCount. Then test the known-good RTC and disconnected operation. With no valid RTC AND no network time, dated recovery must wait; an NVS recording-year value alone is not a trustworthy current date.

The previously identified Gate sd/diag/status/health parity gap remains separate and is not claimed fixed by this targeted recovery release.

Validation completed: 20 host tests passed per repository; 34 shared files matched; both ESP32 compile/link checks passed with the disposable OTA substitute. Licensed local builds and real power-loss retests remain required.
