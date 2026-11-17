# Settings rejection diagnostics — modular-test.11

On November 17, 2026 the installed Car Counter (.10) reported an overnight
`REJECTED_INVALID_SETTINGS`, then recovered to `OK`. HA's recorded CSV payloads
were valid and switched date at local midnight. The original rejection trace
was overwritten. A midnight HA/RTC date mismatch is plausible, not confirmed.
The warning automation creates persistent notifications and does not dismiss
them on recovery. This release does not modify that automation.

## New evidence

`msb/traffic/CarCounter/System/settingsLastRejection` is retained JSON containing:

- `reason`: precise parser/validation failure, `FUTURE_ISSUED_DATE`,
  `STALE_ISSUED_DATE`, or an extension rejection reason.
- `field`: zero-based CSV field index, or -1 for the whole payload.
- `receivedIssuedDate`: decoded field 6; zero if decoding did not reach it.
- `acceptedIssuedDate` and `recordingYear`: effective device context.
- `rtcLocal`: RTC wall time at receipt, in the device's configured Central time.
- `rtcReady`: RTC initialization flag, not a new guarantee of clock accuracy.
- `uptimeMs` and `rejectionCount`: boot-relative evidence (millis wraps normally).

A single RTC snapshot drives issue-date comparison and its timestamp. Evidence
is printed to serial and published before settings health, then repeated on
normal settings publication. Successful settings updates do not erase it.
The broker retains it across reconnects/reboots; RAM evidence and counter reset
on reboot. No old broker evidence is republished from RAM until another rejection.
This is the last rejection, not a historical event log or an active-fault flag.

Existing health strings and the settings CSV format remain unchanged. Rejected
commands do not apply settings, write settings copies, or reset/change counts.
A repeated identical valid payload still has the existing health behavior;
this diagnostic-only change does not clear unrelated save failures.

## Validation and scope

Host tests compile the real operating-settings service with test hardware/storage
interfaces. They cover midnight future-date rejection, recovery, retained evidence,
stale dates, malformed CSV, recording-year mismatch, invalid dates and detector
bounds, alongside existing persistence/reset/summary tests. Original validation
limits remain; widened arithmetic avoids overflow during range checks.
Reviewed behavior hashes and changed-file hashes were updated explicitly.

This release updates Car only. Gate remains on its existing revision; a peer
comparison against that older Gate checkout will flag the three changed common
files (`OperatingPolicy.h`, `OperatingSettings.cpp`, `operating_release_test.cpp`).
Do not overwrite either device's configuration to remove those differences.

## Local build and acceptance

1. Preserve local work, pull `refactor/shared-firmware`, and check the manual
   `FWVersion`/changelog at the top of `src/main.cpp` (.11).
2. Run the host suite and `tools/verify_modular.py` where Python/g++ are available.
3. Build locally with the licensed ElegantOTAPro library. This repository does
   not include that paid library or private credentials; host tests are not an
   ESP32 firmware build.
4. Use the existing controlled firmware deployment procedure when ready. No
   firmware is uploaded as part of this engineering change.
5. After deployment, confirm version .11, normal health and existing totals.
   On the next rejection, capture `settingsLastRejection` with `settingsHealth`
   and HA's sent payload/time. Do not inject invalid settings on production
   merely to exercise diagnostics.
