# Car journal initialization after verified inspection — .21

The operator's .20 report confirms the existing default 4 MiB layout:

| Region | Start | Bytes |
|---|---:|---:|
| OTA 0 | 0x10000 | 0x140000 |
| OTA 1 | 0x150000 | 0x140000 |
| DATA spiffs | 0x290000 | 0x160000 |
| Inspected / reserved journal range | 0x290000 | 0x100000 |

Both journal banks contain non-erased bytes. Bank 0 starts `E9 05 02 20 ...`,
consistent with an ESP firmware-image header, not a journal header. The report
alone does not validate an entire image or establish how it got there. The
operator recalls an abandoned internal-memory or web-server experiment last year.

The region backup was verified on SD and downloaded independently:
- Bytes: 1,048,576.
- FNV32: CC56FC4C.
- SHA256: DE4C066D622782D66ECFDC2EA777677CCAC7565BB2272233DE853ED4E48584C1.
- Counts in the report: enter 22, show 21; these are observations, NOT hardcoded
  initialization values. Any subsequently counted cars are preserved dynamically.

## Action performed by Car .21

**Flashing this release enables one-time initialization of that exact backed-up
DATA region.** It does not merely inspect. Use a quiet traffic gap: successful
initialization performs one additional automatic restart. Reboot downtime cannot
detect vehicles.

Before erasing any byte the firmware must:
1. Find the same approved partition layout and prove the running app is in an OTA
   slot, outside DATA. Both OTA slots and the remaining 384 KiB of DATA stay intact.
2. Match the reserved region's SHA256 to the inspected value, or find a durable
   interrupted-migration receipt for that identity.
3. Locate a full-length SD backup in TrafficJournalInspection matching that exact
   SHA256. Hash it again during source comparison, rejecting deterioration after
   the first successful hash. The operator's downloaded backup stays untouched.
4. Reject any valid journal header or committed journal record, including records
   surviving a damaged header. Never reinitialize a healthy journal.
5. Independently read the actual SD CountState checkpoint and compare its current
   date, totals, reset marker and all hourly/official buckets to live counts.
   Equal-sequence conflicting SD checkpoints are rejected.
6. Persist and read back the migration receipt in its own NVS namespace before
   the first erase. Existing settings, pending events and credentials are retained.

Erasure is limited to 256 individual 4 KiB sectors in that 1 MiB region. Each
sector is read back and checked erased. Beam detection runs between sectors and
SD checkpoint currency is checked before every sector. Losing the baseline stops
the process; the receipt and original backups remain for a subsequent restart.
A resumed image must be compatible with NOR erasure of that exact backup: no
original 1 bit may have become 0. New/different programmed data cannot be treated
as an interrupted erase. This condition plus the journal-record guard prevents
an old receipt from authorizing deletion of newly written journal data.

After erasure, independently verify the live SD baseline again, commit it as the
initial flash journal snapshot (including the retained pending count), and verify
that commit BEFORE restarting. Thus a failed SD mount on the next boot cannot
remove the initial count baseline. Normal recovery loads the committed snapshot.
A healthy subsequent boot marks provisioning READY and never erases it again.
No totals are zeroed; the historical unassigned event remains unassigned.
A power cut during the very first header write can be repaired without erasing:
only the expected header may be completed, only with a valid migration receipt,
verified backup and SD baseline, and only if every other byte in the 1 MiB region
is erased. An existing count record or foreign content prevents this repair.

If a guard fails, journalHealth remains faulted and the specific reason appears
under System/journalProvisioning. No generic remote erase/format endpoint exists.

Gate remains .15 and needs no firmware change.

## Operator steps

In the Car repository on `fix/traffic-reliability-20261122`:

```powershell
git pull --ff-only
pio run
```

Flash the locally built .21 firmware.bin. It initializes the known region and
restarts once. Then confirm:

```text
System/journalHealth = OK
System/journalArchive = OK
System/beamCaptureHealth = OK
System/beamCaptureDropped = 0
System/countPersistence = OK
```

System/journalProvisioning should settle at READY; journalFreeRecords starts near
3249 and decreases with committed events/state records until verified bank reuse.
Keep the .20 backup locally: abandoned firmware images may contain historical
compiled configuration. Do not publish the binary backup or add it to Git.

## Validation scope

The native transaction test compiles the real provisioning service against a
fault-injection flash/SD/NVS interface and real SHA256 implementation. Only its
pinned identity is substituted with an independently hashed fixture. It tests
all 257 erase-sector boundaries, all 345 initial header/record write boundaries,
partially erased bits, new programmed bits,
backup corruption after the first successful hash, wrong layout, running-app
protection, stale/currently changing SD baselines, committed-record protection,
healthy-journal refusal, and actual CountPersistence/JournalStore bootstrap after
the modeled restart. Counts and a preexisting pending event survive that boot with SD unavailable.
The host SHA256 test interface uses OpenSSL; firmware uses the existing framework
mbedTLS implementation and needs no additional device library.
These simulations do not certify physical flash power-failure behavior.

ESP32 compile/link checks use the existing disposable nonfunctional OTA interface;
only the operator's licensed local build is deployable. No hardware erase was
performed by the assistant. Initialization is performed by the operator's .21
release after the specified local build/flash step.

Recorded .21 checks: 34 native tests PASS; structural and shared-file preservation
PASS; ESP32 compile/link and IRAM audit PASS. Disposable compile-contract usage:
RAM 84,880 B; app 1,074,797 / 1,310,720 B. These are not licensed-release size
measurements. The actual deployable image is built locally with ElegantOTAPro.
