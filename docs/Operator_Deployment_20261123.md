# Operator deployment evidence — 2026-11-23

Operator confirmed local licensed builds and flashed Car .19 / Gate .15.
Gate terminal reported SUCCESS; OTA identity screenshots show both versions.

MQTT supplied after deployment:

| Measurement | Car | Gate |
|---|---|---|
| Boot local | 21:19:12 | 21:24:33 |
| Heartbeat local | 21:26:42 | 21:26:32 |
| Daily | enter 22 | exit 3 |
| Show total | 21 | 21 |
| Count persistence | SD_ONLY_FLASH_FAULT | OK |
| Journal | FOREIGN_DATA_PRESERVED | OK |
| Archive | FLASH_NOT_READY | OK |
| Journal free records | 0 (not opened) | 3249 (initial snapshot used one) |
| Beam capture | OK / dropped 0 | OK / dropped 0 |
| Maximum foreground drain gap | 2938 ms | 3542 ms |
| SD health | OK | OK |

Car has no active flash journal: the guard rejected non-erased/unrecognized bytes.
No erase occurred. This does not identify their origin or prove a chip defect.
Gate initialized and archived its initial snapshot. These observations are not
paired vehicle/video acceptance tests and do not establish perfect count accuracy.
Gate in-park 19 is the arithmetic 22 - 3, not independent occupancy evidence.

Car DHT reported OUT_OF_RANGE, seven consecutive failures and no valid reading;
its published temperature/humidity are null. RTC health is OK on both, observed
RTC time loss 0. Car's one historical unassigned event remains separately retained.

Existing Arduino-DS3231 warnings are in unused functions: neither application
calls that library's setDateTime/dateFormat; both use RTClib RTC_DS3231.
Gate's three snprintf warnings concern theoretical out-of-range component widths;
valid YYYY-MM-DD and YYYYMMDD fit. Do not conflate these with the flash-content
rejection or reset counts to clear any health warning.

Follow-up is Car .20 read-only journal inspection and verified SD backup, described
in the Car repository's docs/Journal_Inspection_20261123.md. Gate remains .15;
no Gate firmware update is requested. No unknown internal content may be erased
without inspection/preservation and a specific approved initialization decision.
