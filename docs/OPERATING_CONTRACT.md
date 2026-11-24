# MSB Car / Gate Counter operating contract

**Owner decisions confirmed by Greg on November 24, 2026. This is the authoritative
operating requirement for BOTH repositories. Read this before modifying firmware,
Home Assistant settings, counts, resets, reports or analytics.** Historical notes
that describe 24/7 production vehicle logging are superseded by this contract.

## Season boundary: testing versus production

Everything being collected before the November 27 production cutover is TEST DATA.
The public show opens Friday, November 27, 2026 at **5:00 PM America/Chicago**.
Greg will change the HA show start date to **2026-11-27 on Friday**. That transition
must start a clean production dataset on both counters:

- Zero all active daily/hourly/show count baselines, including accumulated test
  show totals and test pending/recovery events in the NEW production dataset.
- Start fresh active entrance/exit traffic logs, event archives and show summaries.
- Preserve the test dataset separately as an identifiable archive. It must not be
  imported into production, restored into active counts, or included in capacity
  comparisons. Archived test records remain available for diagnostics.
- Coordinate RAM, NVS, SD checkpoints/exports, raw flash journal and recovery
  metadata as ONE restartable/idempotent transition. Deleting CSVs alone is wrong:
  persistent test snapshots could otherwise restore the old totals after reboot.
- Repeated HA date/settings publications and ordinary reboots must NEVER repeat
  the season reset or erase new production vehicles. Repeating an already applied
  date command is a no-op. Do not format OTA slots or erase partitions to reset.
- Transition status and any failure must be explicit. Never advertise a completed
  production reset while only part of the durable dataset has been reset/archived.

This is a production-start action, not a reset on every HA reconnect or daily
settings transaction. The normal nightly daily resets remain separate.

## Official show traffic only

During the production season, only official show traffic contributes to counts,
active vehicle/event logs, occupancy, hourly peaks, capacity comparisons and
nightly reports. Pre-show and post-window raw passages are informational only;
Greg does not want them accumulating in the production traffic logs or totals.
Do not continue 24/7 raw vehicle logging as an unannounced alternative.

Physical beam sensing, occupancy/stuck-beam protection and alarms must continue
working outside the logging window. Disabling logging must not disable sensing
or declare an occupied beam idle. Weather/status diagnostics are distinct from
vehicle traffic records and are not guest counts.

## HA owns the schedule and offsets

**The show start time is always 5:00 PM. HA supplies it.** HA also supplies closing
time, the approved device offsets/grace periods, season dates and extension.
Firmware validates and persists the complete operating-settings transaction for
reboot/MQTT-offline use. Do not duplicate schedule ownership in a separate timer,
a hard-coded firmware clock rule or a new manual occupancy-baseline workflow.

| Item | Required behavior |
| --- | --- |
| Car nightly baseline | Daily entrance baseline zero at opening / 5:00 PM |
| Gate nightly baseline | Zero later, at its HA-controlled opening delay |
| Gate opening delay | Keep HA's existing delay; most recently observed +10 minutes |
| Gate closing grace | Keep HA's existing closing delay; most recently observed +25 minutes |
| Extension | Apply the HA-authorized extension to the windows; normally **0 minutes** |
| Gate drain | Continue official exit counting through its delayed closing window plus approved extension |
| Reboot during a show | Restore accepted counts; never reset them merely because the device rebooted |
| HA/MQTT outage | Continue from saved accepted settings and local durable recording; reconnect must not reset/recount |

### Why Gate's timing differs

**It takes a MINIMUM of 13 minutes to traverse the park at 5 mph.** This is Greg's
operating fact and the reason Gate cannot use the same start/stop window as the
entrance. A +10-minute Gate reset opens its count window before the earliest
expected new guest exit, around 5:13 PM. It is not an instruction to replace the
chosen HA offset with a hard-coded 13-minute offset.

Congestion can make traversal longer than 13 minutes. Gate's closing grace and
approved extended minutes allow the last cars to drain. Do not force occupancy
to zero to conceal remaining cars. A nonzero balance at cutoff needs review;
software must not invent exits or silently grant an unapproved extension.

### Applied-setting discrepancy to reconcile before production

The last supplied Car .29 telemetry (November 24, 09:54 local) reported:

    showStartMin = 1020       # 5:00 PM
    resetMinute = 1019        # 4:59 PM
    resetOffset = -1
    countStartMinute = 1020
    countEndMinute = 1090     # 6:10 PM for the temporary 6:00 PM test close

Thus the official entry window starts at 5:00, but that observed daily reset was
one minute earlier. Exact 5:00 reset requires the existing HA helper
`input_number.carcounter_start_offset_min` to be **0**, not -1. Its automation sends
that helper as field 3 (zero-based) of Config/operatingSettings.

The last observed Car end/grace helper was +10 minutes. If the requirement is to
stop entrance logging exactly when HA closes the show plus approved extension,
`input_number.carcounter_end_offset_min` must be **0**. Gate's delayed end remains
separate and must be preserved. Do not silently change a helper or claim the
applied timing matches the requirement without verifying State/resetMinute and
State/countStartMinute/countEndMinute on the device. This documentation does not
apply a live HA setting or reset any counts.

## Occupancy and capacity analysis

The nightly tracked-guest baseline is **0 at show opening**, supplied by the
existing operating/reset policy. Do not ask Greg to recount an opening baseline
for every night. Entries build it; eligible exits reduce it. Gate's later daily
counter reset must not erase entrance vehicles already admitted since 5:00.

Record/report hourly official entries, official exits, hour-end occupancy and
observed peak occupancy. Administrative resets/snapshots are NOT passages.
Use the firmware's accepted show qualification and actual HA windows; do not
subtract unrelated 24/7 totals or label hourly exits as cars-in-park.
Preserve uncertainty for missing/undated/capture-loss events; never clamp a
negative discrepancy to zero or manufacture missing passages.

Fridays, Saturdays (especially Saturdays), and **December 23** are the important
high-volume comparison nights. Light current tests validate operation only; they
are not evidence of park capacity. Observed peak occupancy is not automatically
a rated/safe capacity limit or a waiting-time model.

## Reports, weather and shared-code rules

- Keep daily show summaries, hourly show buckets, CSV downloads, date selection,
  copy-for-email and print/PDF available. Greg emails reports after each show.
- Car's EXTERIOR DHT22 is the official outdoor temperature source. Gate's sensor
  is inside the metal/battery enclosure and is an unofficial backup, not a
  replacement for official outdoor report temperature.
- Preserve valid show-temperature averages across reboot and report rewrites;
  never fill missing historical samples with today's reading or a guessed zero.
- Common modules, data types, payload schemas and topic suffixes must be identical
  and enforced by peer verification. Device roots, pins, hardware/display roles
  and HA-configured Gate offsets are legitimate device configuration differences.
- Keep the existing OTA update capability. Greg builds locally with licensed
  ElegantOTAPro and flashes each ESP32 himself.

## Implementation status — do not mistake requirements for released behavior

Latest operator-confirmed builds: **Car .30 / Gate .23**, both compiled with zero
warnings and flashed on November 24. Their shared season-folder type and build
warning fixes are released.

**The newly requested production-start transactional reset/archive and strict
show-only suppression of raw vehicle counts/logs are NOT implemented in those
builds.** Their countTheCar path still increments raw totals/logs outside official
show windows; only the separate official series is window-qualified. Changing
showStartDate currently saves the calendar, not an all-storage production reset.
Do NOT change the date on Friday expecting the requested clean cutover until the
new transition has been implemented, fault-tested and explicitly released.

The occupancy report addition is work in progress; it must obey the show-only
scope above. No production reset, live HA helper change, or deletion of device
data has been performed by this documentation update.
