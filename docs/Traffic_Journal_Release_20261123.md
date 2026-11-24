# Traffic journal integration — 2026-11-23

Car `25.12.03.2-modular-test.19`; Gate `25.12.28.0-modular-test.15`.
Branch in both repositories: `fix/traffic-reliability-20261122`.
This release promotes the engineering prototype into `include/traffic` and
`src/services`. The engineering branch's `experimental/journal` remains historical.
The operator's Fritzing commits are retained. GPIO assignments are unchanged.

## Count and event transaction

1. GPIO CHANGE interrupts capture both raw beam levels and a microsecond timestamp
   into a fixed 2,048-entry internal-DRAM queue (2,047 usable). The ISR is allocated
   with `ESP_INTR_FLAG_IRAM`; it calls no detector, SD, I2C, MQTT or allocator.
2. Foreground code replays raw edges through `BeamTimeline`, applying the existing
   50 ms continuous-level debounce by capture time. Long stops retain the passage;
   a stuck alarm does not discard it. Both-clear rearming is preserved.
3. The existing device detector accepts the vehicle. Its timestamp, passage,
   A-to-B follow, B-held duration, inter-vehicle spacing, preceding both-clear
   interval and full resulting count snapshot commit to internal flash first.
   MQTT current totals and time follow immediately, before synchronous SD work.
4. SD checkpoint/text/hourly files remain compatible. Immutable detailed records
   copy to `/CC/YYYY/TrafficJournal/0000000001.bin` or `/GC/YYYY/TrafficJournal/…`.
   Replay copies records; it never increments totals. It advances at most one
   record per foreground service pass and resumes after failure/reboot.
   Bank-wide SD verification waits until that bank has finished archival, avoiding
   repeated large scans while catching up an outage backlog.
5. Show resets and date rollover commit full snapshots, including the reset marker.
   A deadline armed before it occurs is applied in captured-event order, even if
   the foreground skips the whole reset minute. Booting after a deadline does
   not arm a retroactive reset. Reset snapshots cannot consume a staged vehicle.
   Corrections commit their timestamp identity and resulting count in ONE record,
   before NVS acknowledgement. Retry cannot erase subsequently counted cars.

On boot, independently verified SD and internal count snapshots are reconciled by
**CountRecord.sequence**, not the unrelated journal event sequence. Equal sequence
with different content fails closed. Newer flash state restores with SD absent;
SD copyback does not mistake a flash-only restore for an already saved SD copy.
An absent baseline or invalid/backward clock preserves accepted events separately;
no invented day/hour or automatic assignment into official totals is permitted.
Existing unassigned NVS counts are retained. This cannot reconstruct the two cars
reported missed while the hardware was down, or resolve historical unassigned data.

## Capacity, recovery and OTA

No partition table changes. The runtime adapter checks the existing `spiffs` DATA
partition, minimum 1 MiB, alignment, encryption and separation from BOTH OTA app
slots before access. Unknown contents are preserved and reported; no filesystem
mount/format or automatic erasure of unknown contents occurs.

Two 512 KiB banks contain 1,625 records each: 3,250 immutable 320-byte records,
including snapshot/reset/correction records. A 1,300-car night uses about 416 KB
plus state records. This is finite outage capacity, not unlimited storage.
Firmware OTA remains enabled. `setFilesystemMode(false)` prevents filesystem OTA
from replacing this data region; upload the normal **firmware.bin** using the
existing licensed ElegantOTAPro environment. The framework is pinned to the
operator's recorded Espressif32 6.9.0 / Arduino 2.0.17 toolchain.

An old bank may be erased only after every committed record is byte-for-byte
verified on SD and the other bank retains the newest count snapshot. A committed
receipt in that retained bank authorizes retry of an interrupted erase. A stale
receipt cannot erase a reused bank containing newer records. Torn append slots
are consumed, never overwritten. Unknown/corrupt data fail closed. An interrupted
initial bank-header write without a committed ownership/reclaim receipt also
fails closed and requires inspection; it is never permission to format data.
A successful flash commit protects completed events. No firmware can count while
unpowered or preserve a vehicle that has not reached a completed durable commit.

Web mutations cannot edit the journal archive. Read/download remains available.
The SD health timer retries a missing mount with the existing SPI pins/clock,
verifies write/read before declaring recovery, and refreshes its capacity topic.
It does not call `SD.end()` under mounted asynchronous web downloads. Physical
startup power/SD electrical reliability is still an independent open issue.

## Health and limits

Under each counter's `System` tree:

| Topic | Normal | Fault/action |
|---|---|---|
| `journalHealth` | `OK` | Foreign data, layout, flash, corruption or full state needs review |
| `journalArchive` | `OK` | `SD_OFFLINE_FLASH_ONLY` retains flash; SD verify failure retries |
| `journalFreeRecords` | remaining slots | Capacity includes state records as well as vehicles |
| `beamCaptureHealth` | `OK` | ISR failure or overflow means counts need review |
| `beamCaptureDropped` | `0` | Sticky loss indicator; never reported as healthy |
| `beamDrainMaxGapMs` | observed foreground gap | Measures drain stalls, not ISR sample latency |
| `countPersistence` | `OK` | `OK_FLASH_SD_PENDING` means flash is durable, SD is pending |

An SD fault overlays the OLED while retaining count visibility. Flash or beam
capture faults take display priority. Capture overflow discards the uncertain
batch and requires both-clear rearming; it does not fabricate missing vehicles.
A foreground stall of 4,000,000 ms also faults instead of ambiguously interpreting
wrapped microsecond timestamps. Electrical pulses faster than interrupt delivery,
interrupt-disabled intervals, CPU resets and power outages remain physical limits.
The queue and placement checks are protection, not a hardware timing certification.

Detailed binary archives are independent of legacy EnterLog/ExitLog CSVs. Legacy
CSV rows can be absent during SD failure; the journal preserves those event fields.
To make a verified CSV from an SD backup, run with Python 3 (including PlatformIO's
Python environment):

```powershell
python tools/export_traffic_journal.py 'D:\CounterBackup' 'D:\TrafficFlow.csv'
```

The exporter validates all records before replacing output, rejects conflicting
identities and never edits input or counts. Epoch fields encode RTC LOCAL wall
time; conversion deliberately applies no second timezone offset. Binary record
schema 2 carries the cumulative unassigned count and correction identity through
bank reuse. Records from a different recording year are not written into protected
historical folders; an unarchived prior-year backlog requires review before reuse.

## Validation and local build

Native tests execute the real count persistence, operating settings, detector,
ISR queue/timestamp conversion, SD archive adapter and health-check bodies.
Coverage includes 1,300 offline updates, flash-only reboot, show reset, midnight,
manual-correction replay, stale-SD copyback, conflicts, long stopped cars,
100 delayed closely spaced vehicles, timestamp wrap, queue overflow, 313 record
write cut positions, 33 receipt write cuts and 129 erase-sector boundaries.
SD archive tests inject all 320 short-write lengths and read/rename failures.
The exporter verifies timing fields and rejects corruption without partial output.

`tools/check_capture_iram.py` runs automatically after linking. It rejects an ISR,
its queue/state, timer function or direct callee linked into cache-backed flash.
This is an executable placement check; no stand-in binary is a deployment artifact.
Greg builds with local secrets and licensed ElegantOTAPro and flashes locally.

Build commands from the existing repository terminal:

```powershell
git pull --ff-only
pio run
```

The branch remains `fix/traffic-reliability-20261122`; no branch switch, filesystem
image or data reset is needed. Verify the new identity and `journalHealth=OK`,
`beamCaptureHealth=OK`, `beamCaptureDropped=0` after each local deployment.
Do not format internal flash to clear a warning or reset counts to reconcile it.

### Recorded results

| Check | Car .19 | Gate .15 |
|---|---|---|
| Native suite | 31 PASS | 30 PASS |
| Structural/pin/AP/topic preservation | PASS | PASS |
| Identical common journal/capture files | PASS | PASS |
| ESP32 compile/link contract | PASS | PASS |
| ISR, GPIO dispatcher, timer and queue placement | PASS | PASS |
| Contract RAM | 68,368 B | 69,304 B |
| Contract app flash | 1,062,925 / 1,310,720 B | 1,052,965 / 1,310,720 B |
| Licensed OTA operation / physical deployment here | Not performed | Not performed |

Compile/link used real Arduino-ESP32 2.0.17, AsyncTCP 3.3.1 and
ESPAsyncWebServer 3.4.5 with public dependencies; only credentials and the
proprietary ElegantOTAPro interface were nonfunctional disposable substitutes.
Those files and binaries are not published. Sizes above are not licensed-release
measurements. The ordinary operator build retains the actual licensed library.

Existing device-specific DHT/summary/settings diagnostic differences were retained,
not silently synchronized. `tests/device_files.json` pins those files separately;
`tests/shared_files.json` verifies genuinely identical common files across repos.
The Fritzing drawing files remain in their operator-committed locations.
