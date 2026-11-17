# Car Counter .13 — DHT reporting and averages

## Field evidence and changes

November 17, 2026: .12 rebooted at 07:32:47 local. MQTT retained the -998
startup pair until 07:42:47, when it published 44.4 F / 54.5% RH. This proves a
startup-reporting delay, not the cause of intermittent DHT failures seen since
last season. Gate separately reported 40.5 F / 64.9% RH. The Car sensor is inside
an additional plastic enclosure. No calibration or cross-device substitution
is justified by these readings alone; moving it to representative protected
outdoor air is separate physical work.

- One-minute startup/read interval remains nonblocking. No longer waits ten
  minutes to publish the first valid pair or recovery after a failed reading.
- Existing Env/temp keys and topic remain. Unavailable values are JSON null,
  never -998/-999, NaN or an invented zero. Normal periodic publishing remains
  ten minutes; reconnect uses the same validity-aware publisher.
- System/dhtHealth retains JSON status (WARMING_UP, OK, READ_FAILED,
  OUT_OF_RANGE, STALE), cumulative/consecutive failures and last-success uptime.
  Counters/times reset on boot. RTC timestamps are not inferred from uptime.
- Temperature must be finite and -40..120 F; humidity finite and 0..100%.
  A failed pair never adds a sample. The next scheduled read retries normally;
  this does not power-cycle the sensor or claim to repair a physical fault.
- Hourly sums/counts use full local date/hour, not just hour-of-day. Each valid
  read contributes exactly once. 0 F is valid. Hour closure runs even if all
  subsequent reads fail; missing hours are not published as zero.
- DailyAvgTemp now weights valid readings collected during the official
  counting window, including configured grace/extension. It is not an equal
  average of 17..20 hourly averages. No samples means a blank CSV field.
  Existing CSV column order and historical rows remain unchanged.

## Limits

This updates Car only. Gate firmware/sensor behavior remains unchanged; common
file comparisons against its older revision will flag the changed services,
headers and tests. Raw DHT last-good globals remain available internally, but
publishing marks them unavailable after a failure or two-minute staleness.

Temperature accumulators are RAM-only and start afresh on reboot. A post-reboot
summary represents only valid samples collected since boot, not a reconstructed
full-night average. Missing periods are omitted, never imputed. Prior CSV rows
and retained MQTT hourly values are not retrospectively corrected or relabeled.
Existing hourly MQTT topics carry hour-of-day only, so old retained observations
must not be mistaken for today's samples.

HA sensor templates must support JSON null/unavailable values. Check the actual
installed templates before claiming HA chart/long-term-statistics acceptance;
a template that converts null to 0 must be corrected. No HA configuration has
been changed by this release. Historical HA statistics containing old sentinels
are not repaired by firmware.

## Validation and local acceptance

21 host tests pass, including compiled real sensor, environment and summary
services: warmup, one sample per read, first publication, failure/recovery,
invalid humidity, real zero F, sample weighting, hour closure during failure,
full-date rollover, backwards clock rejection, show cutoff and blank summaries.
Preservation checks pass with explicit reviewed behavior/shared-file hashes.
The licensed ESP32 build and field acceptance remain pending.

Build .13 locally with the licensed OTA library. Before deployment check the HA
Env/temp templates for null handling. Use the existing controlled deployment
procedure; no firmware upload is performed as part of this change. After
installation, verify the first valid Env/temp appears around one minute after
boot and dhtHealth becomes OK. Compare normal averages against actual samples;
do not inject sensor faults or reset production counts merely to test this.
