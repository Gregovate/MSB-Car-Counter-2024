# DHT loss after days of operation — Car .22 diagnostics

Operator .21 snapshot (boot 22:39 local, heartbeat 22:40:59):
- journalHealth, journalArchive and countPersistence OK; journalProvisioning READY.
- enter 22 / show 21 retained; journalFreeRecords 3249; beamCaptureDropped 0.
- dhtHealth OUT_OF_RANGE, 2 consecutive failures, no valid reading this boot.
- HA temperature history ends near 18:00 around 32 F. The image suggests the loss
  preceded the .19 boot at 21:19 and .21 installation; exact failure time/raw values
  cannot be recovered from that plotted aggregate alone.

The DHT pin, sampling interval, library call order and validation did not change
between deployed .17 and .21. Archived 2025 source and current DeviceConfig both
configure DHT22 on GPIO4. No pin change is proposed. In this code 32 F and values
just below freezing are valid: the allowed temperature range is -40 through
120 F. The library explicitly decodes negative DHT22 Celsius values before
converting to Fahrenheit. This is not evidence of a freezing-point software cut.

OUT_OF_RANGE means the software received a finite value outside the accepted
range in at least one channel. It does not establish that the entire sensor is
physically dead. Pair validation intentionally rejects both published channels
and the average sample if temperature OR humidity is invalid. Thus a bad humidity
reading can make an otherwise valid temperature unavailable. The old health
message omitted the raw values and rejection field; it cannot identify which.
Do not infer a voltage, wiring or sensor-replacement diagnosis from it.

Car .22 adds evidence, preserving validation/counting/journal behavior:
- System/dhtHealth retains existing fields and adds pin, type, lastReadUptimeMs,
  lastReadTempF, lastReadHumidity and rejection.
- Rejection distinguishes TEMPERATURE_RANGE, HUMIDITY_RANGE, read failure in a
  specific channel, BOTH_INVALID, NONE and NOT_READ.
- Nonfinite raw values are JSON null, never invalid JSON NaN/Infinity.
- Serial records the rejected decoded values each attempt; the existing debug
  topic includes them on the first consecutive failure.
- Measurements remain null while the pair is invalid; suspect values never enter
  hourly/show averages. No guessed correction, widened limits or synthetic sample.

Build/flash Car .22 using the existing branch and local licensed environment.
Allow at least 60 seconds for its first sample, then capture System/dhtHealth
(or the first debug message). No new HA entity or physical wiring change is needed.
Gate remains .15. .22 runs on an already initialized journal and will not provision
or clear a healthy journal. Provisioning is still bound to the one inspected
legacy checksum for devices that have not yet completed that migration.

Native verification tests finite/nonfinite reporting, exact rejected-channel
classification, raw 101% RH with a valid 60 F temperature, GPIO reporting, and
32/31.8 F acceptance. Existing pair rejection and averaging semantics remain.
SDK compile/link uses the disposable nonfunctional OTA interface; only the
operator's licensed local build is deployable. Physical sensor diagnosis awaits
those actual raw readings. Do not publish fabricated temperatures to fill gaps.

Additional Gate .15 evidence at 22:47:53: debug reports -5866.06 F while
published humidity is 60%. This corresponds to -3276.7 C after Fahrenheit
conversion, consistent with the DHT22 decoder interpreting temperature bytes
0xFFFF (sign plus magnitude 32767). The bytes were not directly captured.
One possible explanation is a two's-complement negative encoding on a sensor
variant (0xFFFF would represent -0.1 C), but a corrupt or faulty measurement
can produce the same decoded value. Neither explanation is established.
The near-freezing timing makes this worth investigating; do not automatically
reinterpret all negative values or replace sensors based on this evidence alone.
Gate's -999 published temperature is an invalid-data sentinel, not a measurement.
