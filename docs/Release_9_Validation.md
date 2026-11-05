# Release .9 validation — 2026-11-05

- Both complete ESP32 source trees with six private AP macros compiled and linked successfully for az-delivery-devkit-v4 using espressif32 6.9.0 / Arduino ESP32 2.0.17.
- This was a disposable compile-contract check with dummy credentials and a nonfunctional ElegantOTAPro API substitute. Its firmware.bin is not an installable artifact and is not distributed. The production licensed build is still required on Greg's laptop.
- Source/include bytes compiled match the published release bytes. The existing production platformio.ini and dependency choices were not changed.
- 20 Python regression tests passed in EACH repository. Native C++ tests compile actual service implementations with -Wall -Wextra -Werror and injected SD/connection failures.
- Shared-file verifier passed against the peer: 34 identical shared files. Original GPIO definitions, original five AP registrations plus the optional sixth-AP path, topic constants, detector enum values and unaffected baseline function bodies remain protected.
- Exact private HA YAML settings payloads were rendered and passed to the actual compiled OperatingPolicy parser for both counters and extension 0/15/30. YAML and Jinja syntax, unavailable-helper rejection, date-bound extension expiration, stable unique IDs and unrelated production automation preservation passed.
- HA's own live configuration check, real licensed OTA builds, physical beam tests, seven-car reboot persistence, disconnected operation and evening cutoff/summary acceptance remain pending. No device or live HA configuration was changed during this work.

See Coordinated_Release_9.md for the test and installation sequence, approved operating behavior and rollback constraints. The Gate bag sensor is not implemented in this release; its current GPIO32 name denotes Beam A, and the alternate input/polarity/wiring contract remains unresolved.
