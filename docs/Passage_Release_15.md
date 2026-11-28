# Car .15 — passage qualification and optional HA diagnostics

**26.11.28.2 update.** The historical continuous-overlap rule below was superseded after the opening-night audit. Both activation and Car Detect delays start at A. B must be occupied when the delays have elapsed. Forward ordering and both-clear rearming still apply.


## Observed failures and ownership

November 17, 2026: .14 counted pedestrians and at least one confirmed reverse
passage (26 -> 27). Consecutive reverse passages were inconsistently accepted.
Normal forward cars were also seen counting correctly. The idle pattern stopped
on rejected beam activity. Earlier source at 928d6c6 has the same counting defect:
both minimum-duration comparisons measure time since A tripped; B-first is not
locked out. There is no correct earlier timing implementation to restore.

Only Car is changed. Gate's firmware, thresholds, count windows and HA controls
remain unchanged. Existing GPIO assignments and all-day/official count storage
remain authoritative. No historical total is guessed or automatically corrected.

## Explicit qualification rules

A new passage starts only after both debounced beams have been observed clear.
A must activate while B is clear, then B must activate while A remains blocked.
B-first, simultaneous start, rebreak/ambiguous order and B clearing while A is
still blocked are rejected and locked until both beams clear.

At completion, each debounced beam must have been blocked at least
`minActivationDuration` (existing 100 ms). The longest continuous BOTH-blocked
interval must meet HA's `carDetectMs`/`waitDuration` (currently 500 ms). This
explicit overlap interpretation replaces the defective A-elapsed comparison.
Thresholds are frozen at passage start. The 50 ms debounce is preserved.
Fast legitimate vehicles with less than 500 ms overlap WILL be rejected; the
new diagnostic record supplies measured overlap for evaluating that setting.
Do not raise/lower it based on guesses.

Count once when both beams clear after a qualifying forward passage. Holding
both beams never repeats a count. The blocked alarm does not terminate the
transaction; a stopped car can finish later. There is no fixed cooldown after
a completed passage; six-second arrivals with a clear interval are supported.
Vehicles with no observable both-clear gap cannot be reliably separated by this
policy and must not be claimed individually validated. Booting mid-passage
requires a clear baseline before accepting any event.

Only an accepted passage resets the 30-second idle timer or writes normal arch
feedback. Unqualified/raw activity and rejected passages do not pause the idle
pattern. This means a pending stopped car does not change the arches until its
passage is accepted; alarms still report independently.

This is beam-pattern qualification, not semantic vehicle identification. A
person or group producing exactly the qualifying durations/order cannot be
separated from a vehicle with these two beams alone. Tests reject defined
walking-length/no-overlap patterns; live pedestrian exclusion still requires
field validation. Do not claim that all people have been proved excluded.

## MQTT debug contract

- Command: `msb/traffic/CarCounter/Config/passageDebug`, exact ON/OFF, non-retained.
- Acknowledged mode: `msb/traffic/CarCounter/State/passageDebug`, retained ON/OFF.
- Events: `msb/traffic/CarCounter/Sensors/passageEvent`, non-retained JSON ONLY
  while enabled and connected. No diagnostic CSV, SD writes, offline queues or
  replay. Existing normal accepted-count records remain unchanged.
- OFF on boot AND each MQTT reconnect. Before subscribing, firmware clears the
  retained command and old event on the same ordered MQTT connection, then
  publishes OFF. PubSubClient does not expose retained flags in callbacks;
  the HA command must still use retain:false.
- ON emits a SESSION/WAITING marker so the dashboard does not display an old
  passage as a new-session result.
- Completion records include boot identity, boot-relative event number, RTC local
  start/end, order, acceptance/rejection reason, each blocked duration, total
  overlap and longest continuous overlap, frozen thresholds, count before/after.
  These times are RTC-local Central time, not UTC.
- One BLOCKED/PENDING record is emitted on timeout (if enabled), followed by the
  final COMPLETE decision for the same event when both beams clear. The timeout
  is an observation, not rejection. Events that occur while disconnected/off
  are not recoverable diagnostic history.
- ACCEPT describes physical qualification. If the clock/count system is not
  ready, existing undated-event persistence applies; before/after may remain
  equal. Check countPersistence/unassignedRecoveryCount in that circumstance.

## HA installation — additive, matching Greg's supplied split files

1. Append `home_assistant/car_passage_debug_switch.yaml` to
   `/config/mqtt_configs/switches/switches.yaml` (do not replace existing switches).
2. Append `home_assistant/car_passage_debug_sensor.yaml` to
   `/config/mqtt_configs/sensors/carcounter.yaml` (do not replace existing sensors).
   These are list entries without a new mqtt: wrapper.
3. Check HA configuration before reloading/restarting with the existing method.
   Both entities must appear: `switch.car_passage_debug`,
   `sensor.car_last_passage`. If HA adds a suffix due to a name collision, adjust
   the card to the actual IDs. No existing entity IDs or automations change.
4. Add `home_assistant/car_passage_debug_card.yaml` as one Manual card below
   Enter Timer Variables in the existing Admin dashboard. It shows OFF, waiting,
   or the latest decision, local times, before/after and timing table.
5. The switch waits for firmware acknowledgement; it does not assume success.
   After boot/reconnect it should show OFF. Turn ON only while testing, then OFF.
   Latest event is available to clients already listening, not replayed to a new
   MQTT Explorer session. HA may record diagnostic sensor history while ON under
   its normal recorder settings; no events are generated while OFF.

Official schema references checked November 17, 2026:
https://www.home-assistant.io/integrations/switch.mqtt/
https://www.home-assistant.io/integrations/sensor.mqtt/

## Validation and deployment handoff

25 host tests pass, covering the actual debounce/detector/debug/arch services
with a count callback and real counting/persistence separately in its existing
suite. Cases include consecutive reverse events, reverse->forward, walking-length
and no-overlap events, A-only, long A followed by short overlap, valid forward
pairs six seconds apart, holding beyond alarm timeout, single final count,
boot mid-passage, simultaneous start, wrap-safe millis and threshold boundary.
Debug OFF/ON/OFF/reconnect and non-retained event behavior are checked. HA YAML
parses and its OFF/waiting/decision card templates render. Preservation checks
pin explicit reviewed changes. No licensed ESP32 build was available here.

Firmware version/changelog at top of main.cpp is .15. Build locally using the
licensed ElegantOTAPro library before any device update. Read repository
Runbook_First_Production_Rule.md before production mutations; use the accepted
controlled OTA pattern, preserving existing counts and settings. No automatic
upload, HA installation, count correction or production setting change was
performed during this engineering work.

Field acceptance remains: .15 identity; healthy restored counts/settings; HA
OFF acknowledgement then ON; qualifying forward passage once; B-first reverse
rejected (unchanged count and idle effect); ordinary walkers rejected; slow/stopped
forward completion; OFF ceases passage messages. Review observed overlap before
changing Car Detect ms. Keep the prior .14 binary for rollback under the existing
procedure; rollback reintroduces its known false-count/arch defects.

## November 27, 2026 entrance audit and timing correction

D4 showed 1,023 car entries. EnterLog had 930 records, including one motorcycle-associated passage and 929 matched cars. The corrected hourly entry buckets are 335, 276, 236, 176 and 0. The season adjustment is +93, applied once to Car only. Preserve the original sensor log. Camera-only entries have no measured millisecond beam data.

Car firmware 26.11.27.3 required 500 ms of continuous simultaneous beam blockage. The 2025 detector measured waitDuration from A, allowing B to stay occupied after A cleared. Release 26.11.28.2 restores the timing predicate in the December 14, 2025 Car source (1760015, src/main.cpp lines 2975–3040). B can follow on the sample that clears A. Qualification latches once elapsed time since A meets both configured limits while B is occupied. B clear then counts once. The 2025 compiled default was 950 ms and could be overridden; the supplied 2026 effective setting was 500 ms. This change leaves the configured value alone. Startup clear, B-first/simultaneous-start rejection and both-clear rearming remain deliberate differences from the legacy Car state machine.

### Gate comparison (opening-night Gate 26.11.27.2, db2e303)

Gate begins on A alone. When it sees B and at least minActivationDuration has elapsed since A, it latches a second timestamp. On B clear, Gate requires the time since that latch to meet carDetectMS (reported effective value 100 ms). Car starts both clocks at A and applies no second duration test at B clear. Gate ignores B-first activity while idle and may later start on A alone; Car locks a B-first or simultaneous-start transaction out until both clear. After an accepted passage with residual A, both wait for both-clear. Gate returns to waiting immediately on a short rejected B hold. Late B follow on Gate produces a diagnostic with no timeout reset. Gate detector source: src/services/VehicleDetector.cpp, WAITING_FOR_CAR through CAR_DETECTED. Gate code and settings are outside this PR.

### Evidence from opening night

The video and CSVs establish the unmatched crossings. The flash journal's vehicle records contain accepted passage timing. Completed rejected passages can be emitted to Sensors/passageEvent when debugging and MQTT are both active. These messages are non-retained and are not appended to SD. A saved MQTT/HA capture, if one exists for November 27, could provide per-event reasons; the after-midnight OFF snapshot does not establish whether debug was enabled earlier. The supplied files do not identify the electrical reason for each miss.

For field acceptance compare individual D4 crossings with the next EnterLog. Include close arrivals, a short overlap, a stopped vehicle and reverse traffic. Enable passage debug during the check to capture rejected decisions and timings. A successful build or an arch flash alone cannot verify physical counts.

Two beams alone cannot distinguish a pedestrian pair from a car when their beam transitions are identical. Include pedestrians in the field comparison.

Before the next show, if Car Cars/ShowTotal is still 930 and the correction has not already been applied, the existing firmware accepts a non-retained MQTT publish on msb/traffic/CarCounter/Config/countCorrection. Payload: local RTC YYYYMMDDhhmmss,show,1023 (timestamp no more than 60 seconds old). Check the next heartbeat's show field for 1023 and the current day's daily total for its previous value. This updates the season checkpoint. It does not rewrite the archived November 27 log or nightly summary. If additional official entries have accrued, reconcile the current baseline before issuing an absolute correction.
