# DHT22 below-freezing compatibility — 2026-11-23

Car .22 evidence: decoded -5860.66 F, RH 66.8%, TEMPERATURE_RANGE.
Gate .15 evidence: decoded -5866.06 F, RH 60%.
Operator reports 27 F at home. These readings correspond respectively to
0xFFE1 and 0xFFFF temperature words under Adafruit sign/magnitude decoding.
Interpreting those words as signed two's-complement tenths-C produces 26.42 F
and 31.82 F. Raw bytes have not been separately captured; words are inferred.

First-hand report of this sensor variant, including below-zero transition:
https://github.com/adafruit/DHT-sensor-library/issues/182
The source library checks checksum before returning a finite decoded sample.

Car .23 and Gate .16 share DhtCompatibility.h: accept the alternative encoding
only for the otherwise impossible decoded range mapping to -40..-0.1 C.
Normal positive and standard sign/magnitude negative values are unchanged.
NaN, sentinels, other out-of-range results and humidity failures remain faults.
No library cache edits, partition changes, timing changes or counting changes.
Car retains raw readings and reports encoding TWOS_COMPLEMENT_COMPAT when used.
Gate adds raw decoded values and encoding to System/dhtHealth; its existing
sampling/publication schedule and range/sentinel policy otherwise remain.
The correction is observable, not a claim of independent sensor calibration.

Build each firmware locally on fix/traffic-reliability-20261122 and flash as
usual. After at least one minute, inspect Env values and System/dhtHealth.
Car should now show approximately 26.4 F for the supplied packet, rather than
null; actual temperature may change. Journal/count data must remain intact.
Existing settingsLastRejection is dated Nov22 and unrelated to DHT decoding.

Verification: exhaust both encodings from -40..80 C, preserve standard readings,
reject NaN/sentinels/other faults; actual Car averaging/publication path tested
using the supplied -5860.66 F / 66.8% RH example. Native suites and disposable
SDK compile checks supplement, not replace, operator hardware verification.
