# Car .16 — restore camera-visible count feedback

Greg confirmed the required visual contract: red means a qualified vehicle is
actively in progress; green means its dated count increment succeeded.

.15 removed the red qualification indication, delayed acceptance until both
beams cleared, and set green only after synchronous saving/logging returned.
.16 restores red once forward timing qualification succeeds and accepts once
when B clears. Both-clear is required for rearming, not for counting. B-first
reverse lockout and timing checks remain. Qualified held cars do not repeat.
Unqualified/rejected short passages leave the idle pattern unchanged.

Green is written immediately after daily/hourly/official count updates in
countTheCar(), before SD checkpoint/export operations. It indicates count
acceptance, not a guarantee of SD durability; System/countPersistence remains
its authority. If dated counts are blocked, undated recovery still applies and
no false green count-success transition is made. The separate unassigned
recovery count must not be added to dated totals without reconciliation.

25 host tests and structural preservation checks pass. Includes red while a
qualified car is held, green/count on B-clear with A still held, no repeat,
reverse pairs, walking-length rejection, six-second cars and long stops.
Licensed ESP32 local build and physical tests remain required.

## Radio acceptance

After the usual licensed build and controlled OTA update, verify .16 and unchanged
restored totals/health. Forward: red after qualification, green/count once at B
release. Reverse: no count. Two successive forward cars: one each. Hold a
qualified passage: red stays, no repeated count, then green once on completion.
Leave thresholds unchanged during initial comparison. .15 debug toggle remains
compatible. No counts were reset and no firmware uploaded by this source change.

## Current firmware GPIO reference (Car only)

| Function | GPIO |
|---|---:|
| Beam A | 33 |
| Beam B | 32 |
| Red arch output | 25 |
| Green arch output | 26 |
| DHT22 data | 4 |
| I2C SDA (RTC/OLED, default) | 21 |
| I2C SCL (RTC/OLED, default) | 22 |
| SD SCK (default SPI) | 18 |
| SD MISO (default SPI) | 19 |
| SD MOSI (default SPI) | 23 |
| SD chip-select | 5 |

This is a firmware pin reference, not a verified as-built schematic or connector
pin-order drawing. Greg re-crimped loose 22/23 contacts November 17; that is
separate hardware evidence from the confirmed feedback regressions.
