# Recording year and show calendar — 2026-11-04

Status: source implementation and automated validation; licensed-library build and hardware acceptance pending. No device was updated by this change.

## Accepted operating rules

Greg requires calendar-year recording, with no writes to an earlier year's folder. The show starts on Black Friday (the day after the fourth Thursday in November), unless HA supplies a valid override for the current recording year. Recording year and show start date are independent. This supersedes the old Car show-date-year rule and Gate November–October rule.

Both applications use byte-identical RecordingPolicy.h and RecordingCalendar.cpp. Device adapters retain /CC and /GC, existing filenames/CSV formats, GPIOs and detector timing. The migration floor is explicitly 2026; existing 2025 records are neither imported nor reconstructed.

## Persistence and clock behavior

- ESP32 Preferences namespace `traffic-cal` holds the high-water `year` and accepted `start` date. Ordinary application firmware uploads preserve this NVS state; a full flash erase does not.
- `/recording-calendar.a` and `/recording-calendar.b` on SD hold independently checked year/month/day/checksum records. Each replacement is read back before replacing the second copy. NVS is preferred for same-year date conflicts; a verified higher SD year prevents regression after an NVS erase. With no NVS date, the first valid same-year SD copy supplies the date.
- A valid RTC advances by one calendar year. A larger forward jump needs a valid network-clock year. Neither source can decrease the high-water year. Invalid/lost-power RTC uses the saved year (or the explicit 2026 migration floor on initial installation) and reports a calendar fault. Invalid clock timestamps still need operator correction; a saved year is not a replacement clock.
- At a year change, both applications update paths without reboot and calculate that year's Black Friday fallback. Old HA dates cannot change the folder or replace the new fallback. A year/path change does not itself reset counters.
- NTP is retried every 60 seconds after startup when connected; a valid numeric date updates the RTC. Existing timezone/offset configuration is unchanged.
- This revision does NOT yet remove blocking Wi-Fi startup/reconnection. The calendar's saved-year recovery is independent of MQTT, but full offline counting still requires the separate network-service correction.

## Show calendar and MQTT

2026 default: 2026-11-27. 2027 default: 2027-11-26. Before opening day, days running is zero; opening day is one; the existing Christmas Eve exclusion is shared. Both devices calculate days running locally. Gate no longer accepts Car's retained days-running value as authoritative.

Both still receive the existing `msb/traffic/CarCounter/Config/showStartDate` command, require an exact valid YYYY-MM-DD in the recording year, and persist accepted changes. The firmware no longer publishes a date onto that command topic. Accepted dates are reported at each device's `State/showStartDate`. Update HA date-state displays to use State; do not mistake a retained old Config command for the accepted state.

`System/seasonYear` and `System/seasonFolder` now report the effective paths repeatedly. `System/calendarHealth` reports OK or calendar/clock/persistence failures. Persistence errors are latched until reboot/recovery review; this is not a complete SD count-write health monitor.

Pre-show resets, show counting and summary scheduling now require the shared active-show calendar, including the Christmas Eve exclusion. Existing daily clock times, reset offsets, and detector timing are unchanged. Setup traffic before Black Friday still uses ordinary daily/hourly traffic recording; it must not be classified as show traffic merely because the clock is after 17:00.

## Historical file protection

All active application SD opens and mutations use the shared guard. Prior/future-year paths under /CC or /GC are read-only; parent-directory removal/rename is denied. Ambiguous paths (case-insensitive annual paths, dot segments, backslashes, repeated slashes, trailing dots/spaces and encoded paths) cannot bypass the guard. Historical downloads remain available.

Web uploads check each chunk and use a per-request file handle. Historical upload/delete/rename requests return 403. Internal calendar recovery records are not editable through the file manager. This does not make a physically removed SD card read-only on a PC.

## Recovery scope and outstanding work

MQTT connection now occurs after SD restoration in both setup sequences. Car restores its show total in the new-day boot branch as well as the same-day branch. No migration of the official 2025 metrics is performed.

This is a bounded calendar/storage-protection release, not completion of the full reliability refactor. Remaining work includes nonblocking network startup/reconnection, all HA timing/offset persistence and acknowledgement, per-vehicle durable save/error handling, complete full-date boot recovery, persisted once-per-day reset/summary markers, SD failure display/alerts, and Gate off-season sensor-mode switching. Current daily-reset and failure-recovery behavior is not certified lossless. Gate's missing second beam still blocks detector hardware acceptance.

## Validation and operator sequence

- Run `python tools/verify_modular.py --peer <other-project>` in each checkout. Original baseline hashes remain intact; `tests/behavior_function_hashes.json` pins the explicit behavior revisions. GPIO/topic definitions, state defaults, enums and shared files remain checked.
- Run `python -m unittest discover -s tests`. Calendar behavior tests require g++ with C++17. Windows operators without g++ may run `python -m unittest discover -s tests -p test_verify_modular_portability.py` and rely on the recorded host calendar test evidence.
- Automated evidence: 11 tests per project PASS (8 calendar/guard tests plus 3 existing workflow tests); Black Friday compared with Python's calendar for every year 2026–2099. Tests exercise the real shared service with host RTC/SD/NVS doubles, including backward clocks, January rollover, old MQTT dates, SD fallback, interrupted config writes and historical file protection.
- Both complete ESP32 compile/link checks PASS with cached public dependencies and the explicitly compile-only OTA interface stand-in. No licensed OTA behavior was tested and no stand-in binary may be uploaded. Existing platformio.ini, local secrets and the licensed ElegantOTAPro remain the real build path.
- Next operator step: inspect a clean local branch and pull refactor/shared-firmware, then build with the licensed library. Do not flash merely because the source tests passed. Preserve the complete current SD contents, especially official 2025 summaries, before the later controlled hardware test. Never test year rollover by changing the production clock; use an isolated spare-card/device test.

The manual version and newest-first changelog remain at the top of src/main.cpp. No version automation has been introduced.


## Count recovery follow-up — 2026-11-04

The `.4` source adds shared dated per-vehicle checkpoints and protected hourly CSV replacement. See [Count persistence](Count_Persistence.md) for evidence, failure handling, remaining limits, and the controlled acceptance procedure. Neither device has been uploaded with this revision.
