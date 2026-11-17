# Car .14 — daytime idle arches

Field report November 17, 2026: .13 counted 16 all-day vehicles at 10:19 local,
but its idle pattern was disabled. main.cpp gated it with officialShowNow(),
which excludes daytime and dates before the show. Greg confirmed that the
pattern must be testable whenever powered.

The pattern now starts after 30 seconds of inactivity independent of show hours
or season eligibility. Either raw/stable beam, an active detector state or a
stuck-beam alarm suspends it and restarts inactivity timing. On leaving an
active pattern the service restores red LOW / green HIGH before detectCar()
runs. The detector then has final output control. Its pins, thresholds, counting,
checkpoint persistence and official-show eligibility are unchanged.

23 host tests pass, including the actual arch service at the 30-second boundary,
raw-beam interruption before debounce, detector output ownership, partial events
and alarm suspension. Preservation checks pass with explicit reviewed hashes.
Licensed local ESP32 build and physical arch acceptance remain pending.

Build .14 using the existing licensed local OTA library and controlled firmware
update procedure. After deployment verify identity/version and healthy restored
counts. With both beams clear wait 30 seconds: the idle sequence should run.
Drive through: detection must interrupt it and count the vehicle once. Wait
another 30 seconds after the passage clears: the idle pattern should resume.
Official show counts must remain unchanged during daytime/pre-season testing.
No device upload or production reset was performed in this engineering work.
