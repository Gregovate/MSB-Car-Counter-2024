# Traffic MQTT contract — HA, Car Counter and Gate Counter

Status: implemented reporting corrections in modular-test.8; operational configuration completion pending. User's HA configuration is private and is not copied into either public firmware repository. This document contains only device interface names and implementation status.

## Naming rules

Use `msb/traffic/<Device>/<Group>/<field>`, with exact device names `CarCounter` and `GateCounter`. MQTT names are case sensitive. Never add a slash when the base already ends with one.

- `Config`: HA-owned configuration commands. Firmware must not publish defaults onto a command topic.
- `State`: accepted/applied configuration, published by firmware. Matching the command suffix makes request versus actual value explicit.
- `System`: firmware identity, connectivity and storage/configuration health.
- `Cars`: counts and hourly snapshots. Keep established count names and HA unique IDs for compatibility.
- `Sensors`: beam events and alarms. Preserve existing aliases while any future renaming is staged.
- `Calendar`: legacy compatibility state. Do not silently rename different existing capitalization in Gate and Car; introduce any replacement with a consumer migration.

A topic defined in a header is not an implemented interface until a publisher or handler actually uses it. The old Gate JSON macro existed without a publisher; .8 fixes that.

## Implemented hourly contract (.8)

| Relative topic | Direction | Payload / meaning |
| --- | --- | --- |
| Cars/hour/json | Firmware to HA | Retained JSON array of 24 all-day hourly counts, indices 00 through 23 |
| Cars/hour/00 through Cars/hour/23 | Firmware to HA | Retained integer for the named hour |
| System/hourlyExport | Firmware to HA | Last SD export result; not a guarantee of MQTT delivery |
| System/countPersistence | Firmware to HA | Count checkpoint/recovery health |
| System/calendarHealth | Firmware to HA | Recording calendar health |
| State/showStartDate | Firmware to HA | Accepted current-year date, YYYY-MM-DD |

Both devices use one snapshot publisher on heartbeat. Gate per-event, load and save publishers now use a single separator. Offline snapshots and failed-restoration defaults are not published by the snapshot helper. Existing broker values on malformed old topics are not automatically deleted. Legacy queue replay remains a separately documented gap.

The packet buffer is grown to 512 bytes only if needed; allocation failure skips the snapshot and emits a diagnostic. The full 24-element payload is never deliberately truncated. API reference: https://pubsubclient.knolleary.net/api . Delivery still depends on the MQTT connection; status publication is not an acknowledgement from HA.

## Tuning contract (.7, retained in .8)

Under each device prefix, Config commands and State reports use these established suffixes:

| Device | Suffix | Units | Handler |
| --- | --- | --- | --- |
| CarCounter | carCounterTimeout | milliseconds | Existing handler; clamped 5000–600000 |
| CarCounter | beamWaitDuration | milliseconds | Disabled; State reports actual firmware default, not successful HA application |
| GateCounter | gateCounterTimeout | milliseconds | Existing handler; validation/persistence still pending |
| GateCounter | carDetectMS | milliseconds | Existing handler; validation/persistence still pending |

Do not rename the last two detector thresholds as though they measure the same event: Car checks duration with both beams high, while Gate checks a beam broken-duration condition. Consistent naming must retain that semantic distinction. A common threshold interface requires a documented adapter and tests for each detector, not merely renamed variables.

## Configuration completion requirements

Operational show dates, start/end times, reset offsets, stop-recording offsets and tuning must have a complete HA helper → command → validation → persisted record → applied State chain. Retained broker commands are not an offline SD backup. Explicit count reset commands must remain separate from setting changes.

Outstanding inputs: the private HA helper definitions and configuration include structure, plus any scripts/packages that publish settings. The supplied automations do not publish reset or stop-recording offsets, and no safe entity-ID mapping can be inferred solely from dashboard labels. Firmware Gate reset remains 17:08, while the supplied dashboard shows +10 minutes after opening. This is not fixed by the .8 reporting release.

The delayed gate reset excludes volunteers leaving after gate opening and before first visitors arrive. All-day park hourly data must remain intact. Existing 2025 traffic indicators and official show summaries are not redesigned or reconstructed here. MSB-Show-Controls remains the governing design repository; publication of its separate draft is still blocked by repository write access.

## HA candidate changes and installation boundary

The private candidate sensor files correct the Car log duplicate/device/topic, show accepted start dates via State, change Gate raw hourly input to Cars/hour/json, and expose health/applied-tuning diagnostics. All existing distinct unique IDs are preserved. The two online sensors per device retain their IDs and timeout values, with labels clarifying 90-second versus five-minute checks; deciding which dashboards should keep requires an entity reference audit.

Gate applied carDetectMS State requires .7 or newer; Gate JSON requires .8. Do not install a partially matched HA/firmware set and interpret absent data as a device fault. Switch commands and automations are unchanged in this candidate. Full HA validation must run against the private live configuration, not only these include fragments.

Hardware procedure: use Count_Persistence.md. After the agreed firmware update, inspect versions and canonical hourly topics read-only; compare JSON indices with individual hour topics, then verify health and a controlled count/reboot. No count resets, broker retained deletes or historical file rewrites are part of this migration.
