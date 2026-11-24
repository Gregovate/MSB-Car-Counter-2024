# Gate build warnings and shared season-folder contract — 2026-11-24

Car .30 / Gate .23 address the operator's successful but warning-producing Gate
.22 build. Do not deploy the older Gate build with the String/%s status defect.

## Root cause and common contract

FileManagerService.cpp was shared, but RuntimeState exposed different types:
Car char[32], Gate Arduino String. Passing Gate's String through snprintf varargs
to %s is a real type error and could corrupt the response, not harmless noise.
Both devices now declare extern SeasonFolder seasonFolder, with the SAME shared
SeasonFolder alias (char[32]). Gate's path construction uses bounded snprintf;
String operations explicitly construct a String only when concatenation needs it.
The original per-device initial folder values are preserved. Directory roots,
filenames, counts, RTC policy, partitions and OTA layout remain device-specific
or unchanged as appropriate. The status formatter also uses a local normalized
String and c_str(), making it robust to either model and future adapter changes.

The verifier explicitly migrates ONLY the old seasonFolder declaration type when
comparing original RuntimeState tokens. It still verifies all original initializer
values/order and detector enums. The shared alias and declaration are checked;
no baseline config is silently rewritten to accept arbitrary state changes.

## Other warnings

Gate date/day formatting buffers in RecordingCalendar, StorageService and
OperatingSettings are expanded to 24 bytes, matching the Car approach. Field
widths in printf are minimum widths, so uint8 RTC components can require more
than two characters. Valid formatted dates and settings payloads are unchanged.
RecordingCalendar.cpp is now byte-identical and promoted to shared verification.

Arduino-DS3231 (jarzebski) is removed from BOTH lib_deps lists. Source and existing
validation records show no application calls to its setDateTime/dateFormat or
DS3231 class. Both programs use Adafruit RTClib's RTC_DS3231, which remains in
dependencies and source. The active RTC driver and time/backup-power behavior are
not replaced. platformio.ini is now also enforced identical across the repos.

## Verification / operator build

The actual file-manager service compiles under -Werror with both char-array and
legacy String seasonFolder mocks. The Gate storage adapter test compiles actual
path-building bodies with the shared char[32] type, validates SD-offline calendar
paths and normal seasonal paths/filenames, and preserves initial values. Verifier
portability fixtures include the shared dependency file. Both full native suites
must pass. The local licensed ESP32 build remains the final SDK compile check.

In each local repository, on fix/traffic-reliability-20261122:

    git pull --ff-only
    pio run -t clean
    pio run

Clean removes build products, not SD data or ESP flash. Flash the resulting
firmware normally after the build succeeds. Never run erase_flash or uploadfs for
this change. No SD UI-file upload or manual partition change is required.
