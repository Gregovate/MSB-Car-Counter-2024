# Traffic reliability correction — 2026-11-22

## Field evidence and scope
Greg's entrance video identifies the 17:33:44 truck held until 17:43:26, no matching accepted local-log event, and idle flashing while occupied. The exact rejection path is not proved: the old passage diagnostics were MQTT-only and were not supplied. These changes correct demonstrated code defects; they do not claim a proved cause for that missed passage.

2025 source: Library EnterLog.csv (758188 bytes), SHA256 78573bcf3e0843f4b97779636742b294e262f6c9341a8985d723eaa134fbe489. The 2025 subset contains 19,626 accepted records across 26 dates December 5–31. Median recorded interval 11.322 s; 7,822 <=10 s, 2,218 <=7 s, 283 <=5 s. Timestamp-resolution subset independently has 57 adjacent pairs <=5 s (minimum 2 s). Most timestamps are only minute-resolution and the interval column sometimes differs from adjacent timestamps, so the smallest interval is not an exact hardware timing requirement. Passage median 3.005 s; 64 passages >30 s, five >60 s, max 120.934 s. Peak fixed calendar buckets: 9/minute, 35/five minutes, 353/hour. These are accepted event records, not proof of zero missed vehicles.

## Changes
Car .17: raw or stable occupancy, any unresolved passage, and alarm suppress idle output. Thirty-second idle delay restarts throughout occupancy. Preserve forward overlap through A clear/rebreak with B held; still reject reverse, simultaneous startup and unqualified short overlap. No timeout discards a qualified passage.
Gate .13: accept at B-clear rather than waiting one extra loop; preserve the long-stop alarm without reset. Residual A must clear before rearming, with no cooldown. Compute spacing before count/log so the CSV records this event's interval, not the previous interval. Magnetic sensor remains diagnostic only.
Both: attempt event-driven count MQTT publication before the synchronous SD checkpoint verification. Keep per-vehicle verified checkpoints and all recovery protections. Poll beams at loop entry and between maintenance calls.

## Validation and limitations
Native regression suites cover actual detector services, 582-second holds, A rebreak, residual A, 100 close forward passages with 100 ms clear spacing, reverse/short overlaps (Car), current Gate interval, torn checkpoints, cold reboot recovery, invalid clock, midnight, prior-year protection, and count reset replay. Run exact checkout tests and structural verification before local ESP32 build.

No licensed ESP32 build or device upload was performed here. PlatformIO and the private ElegantOTAPro library are unavailable in this environment. No binary is supplied. A successful native suite is not a hardware timing measurement.

IMPORTANT: foreground SD writes, DHT/I2C operations and MQTT socket writes remain synchronous. More frequent polling reduces inter-operation delay but cannot sample beams during an individual blocked call. This release must NOT be represented as eliminating all blind intervals or guaranteeing physical counting accuracy. Publication ahead of checkpoint also means MQTT may show a newly accepted count before its checkpoint completes; sudden power failure during that interval may restore the prior count. Existing durability health remains authoritative. Raw-edge capture or a dedicated sampler with bounded event buffering requires separate design and validation; it is not silently introduced immediately before opening.

Local evidence logs are still accepted-count records; complete durable rejected/pending passage analytics are not implemented by this patch. Do not claim otherwise.

## Local build and controlled deployment
Use the repository's existing licensed PlatformIO environment, secrets.h and ElegantOTAPro installation; never copy credentials into Git or this handoff. Record the successful versioned build before OTA. Preserve/download current SD files and counts first. Use the existing device-specific OTA page on Car 192.168.3.31 or Gate 192.168.3.30. Update one device, confirm its new firmware identity, local time, restored counts, countPersistence and hourlyExport, then proceed to the other. Do not reset totals. Firmware .17 (Car) / .13 (Gate) contains no SD record schema migration.

At minimum, operator acceptance must include one held forward passage (alarm without idle/count loss), one normal completion counted once, a closely following pair, and retained totals after reboot. These tests remain pending and must not be called completed.
