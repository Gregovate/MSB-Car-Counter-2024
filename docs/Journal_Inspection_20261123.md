# Car journal foreign-data investigation — .20

Observed on the operator's deployed .19:
`journalHealth=FOREIGN_DATA_PRESERVED`, `journalArchive=FLASH_NOT_READY`,
`countPersistence=SD_ONLY_FLASH_FAULT`; SD health OK; enter 22, show 21;
beam capture OK with zero queue drops. Gate .15 journal and archive are OK.

The journal's content guard found either an invalid bank header or non-erased
bytes elsewhere in a bank whose header appears erased. This is not enough to
identify old SPIFFS/filesystem contents, a partial header or another cause.
It does not prove an internal-flash hardware defect. The region was not erased.
Car currently has verified SD persistence but no active internal event journal.

Car .20 adds inspection only. Gate remains .15 and needs no update.
No counting/reset policy, pin or partition layout changes are made by .20.

1. Build and flash Car .20 using the existing licensed environment.
2. From that repository's PowerShell terminal, run:

```powershell
.\tools\Collect-CarJournalInspection.ps1
```

The device copies the first 1 MiB of the approved journal DATA region into
`/CC/2026/TrafficJournalInspection/flash-<boot-random>.bin`. One 4 KiB block is
transferred per loop, with beam draining between blocks. The backup is compared
byte-for-byte with source before it is published. Failures preserve pending
files and report a specific error; existing names are never overwritten.
The script downloads the verified backup, checks length and FNV32 against the
source, and records a local SHA256. Local output is under Documents /
MSB-Counter-Backups, outside the Git checkout.

Send `CarJournalInspection.json` for analysis. Keep `CarJournalRegion.bin` locally;
no need to upload the binary initially. The report contains actual partition
addresses/sizes, both bank headers, first non-erased offsets/samples, occupied
byte counts and backup state. It cannot identify every old file from a header
alone. NVS and both OTA applications are outside the inspected/backup range.

Routes, independent of the SD-hosted UI:
- `GET /diagnostics/journal`: read-only report, available even if SD is offline.
- `GET /diagnostics/journal-backup`: streams the verified SD backup, otherwise 503.

There is NO internal-flash write, erase, format or initialization route. .20 does
not activate the rejected journal. Any migration/initialization decision follows
inspection and preservation of the existing region. No counts are reset.
The unrelated DHT OUT_OF_RANGE condition remains separately reported.

Validation: native tests execute the real bounded backup worker, check exact
source preservation and successful backup, inject short SD writes, source-read
failure and corrupted SD readback, and verify there is no erase/write route.
ESP32 compile/link and IRAM audit use a disposable nonfunctional OTA interface;
only the operator's licensed build is deployable. PowerShell execution itself
requires the operator's Windows environment and was not run on this Linux host.
