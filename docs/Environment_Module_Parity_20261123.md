# Shared environment modules — 2026-11-23

Car .24 / Gate .17 replace the divergent DHT services with identical sources.
Shared and peer-hash enforced: TemperatureSensor.cpp, EnvironmentService.cpp,
EnvironmentService.h, EnvironmentPolicy.h, DhtCompatibility.h, ShowSummary.cpp,
and the actual-service environment tests. Both publish the same dhtHealth schema.
Device MQTT topic roots remain configured separately; Gate retains tempHumidity
and Car temp. Screen hardware, beam detection, magnetic diagnostics and vehicle
roles remain device-specific. No GPIO, partition, OTA or journal schema changes.

Gate now uses the same validity-aware unavailable/null output, 60-second sample,
10-minute periodic publication, immediate failure/recovery publication, and
reconnect health publication as Car. Averages accept complete valid pairs only,
close hours even during failures, separate full local dates, preserve true zero F,
and weight official-window samples equally. Missing summary temperature is blank.
The compatibility correction remains bounded and raw values remain visible.
Gate's previous sentinel output and hourly-only summary temperature algorithm
are intentionally replaced with the already tested common environment policy.
Shared service owns periodic publication; neither main loop implements a second
cadence. Both reconnect paths call the common publication function.

Physical confirmation: operator Car .23 reported 26.4 F /68.5% RH, health OK,
zero failures and TWOS_COMPLEMENT_COMPAT. Gate .16 also reports compatibility
active (raw decoded -5864.44 F). This validates correction activation; it is not
independent calibration. After this update confirm equal health field sets and
plausible corrected temperatures on both devices after the first minute.

Remaining pre-existing device-specific files are enumerated in device_files.json;
this release does not claim every module in the repositories is now identical.
Do not exempt common modules from parity checks to hide later drift.
