# Nightly reports and binary housekeeping — 2026-11-24

Car .27 / Gate .20 correct the .26/.19 home-page report-access regression.
The CSV exports and /ShowSummary.csv and /ShowSummaryBuckets.csv URLs were not
removed. Existing consumers of those CSV endpoints keep the same paths/schema.
Both shared pages now link to Daily Show Reports and Hourly Show Buckets as well
as CSV downloads. The report viewer is embedded in firmware, defaults to the
latest report date, supports selecting a night/all dates, and offers Print / Save
PDF and Copy report for email. The copied section includes device and report/date
headings. Copy uses browser selection/clipboard; if browser copying is refused,
press Ctrl+C on the selected report. The firmware does not itself send email or
modify the existing nightly HA email automation. Report data still requires SD.
No index.html upload is needed; normal firmware OTA updates the shared UI.

The deprecated beginResponse_P call is replaced with beginResponse, as requested
by the operator's installed ESPAsyncWebServer API. The PROGMEM page remains
embedded. Obsolete SD report-view handlers were removed; shared handlers serve
both viewers and both device builds use byte-identical source/pages.

## Explicit deletion after backup

Select the relevant directory, enter the binary file name in the Download file
field, download/back it up, then use Delete named download file in Maintenance.
For recognized ClosedCounts-YYYYMMDD.bin or 10-digit journal .bin filenames, a
second confirmation asks whether the backup has been made. No automatic purge,
retention timer, wildcard deletion, or count reset is enabled by this release.

The existing protection for uploading/renaming recovery files is unchanged.
Only the explicit deletion path can invoke the following narrow checks:

- ClosedCounts: exact current-season path and valid dated 224-byte snapshot;
  strictly before the active current day; no pending rollover snapshot of that
  date; healthy clock/count persistence/SD; independently reread current-day
  CountState checkpoint at least as new as the saved baseline; snapshot sequence
  older than that baseline. Corrupt, future, wrong-year and active files refuse.
  ClosedCounts is not read by reboot restoration; it is an archival daily copy.
- TrafficJournal: exact current-season/TrafficJournal/10-digit.bin path; healthy
  initialized durable journal and SD/archive; valid full record/checksum with
  matching filename sequence; a dated prior-day record; a current-day durable
  count snapshot; and sequence strictly below the minimum sequence still present
  in BOTH flash banks. This boundary is zero/unavailable if the journal is not
  ready. Records still required for bank-reclamation verification always refuse.
  Undated recovery records always refuse. Deletion never erases flash or changes
  a count, pending count, archive cursor, or sequence.

A refusal means the record is still required or cannot be proven obsolete.
Do not bypass it by renaming, overwriting, formatting or deleting CountState.a/b.
CountState, settings, current-day snapshots, export fallback files, provisioning
backups and live-bank journal dependencies remain protected. Retired journal
records still contain useful traffic analytics; retain the downloaded binary or
its verified exported dataset before deletion. Aggregate CSVs alone do not
preserve per-vehicle timing records.

Native checks exercise actual cleanup methods: date/path/CRC/read failures,
pending rollover, missing current checkpoint, offline/clock faults, undated event
protection, live-bank dependency refusal, retirement after verified reclamation,
and reboot/count preservation. Report tests exercise both URLs, date filtering,
quoted CSV/zero/missing cells, copy/print controls and embedded-page equality.
The operator's local licensed ESP32 build is the final firmware compile check;
this renewed workspace does not contain the prior SDK or private OTA library.
