# Environment MQTT layout — Car .25 / Gate .18

Primary topics differ only by the device root:
- msb/traffic/CarCounter/Env/temp
- msb/traffic/GateCounter/Env/temp
- <device root>/Env/temp/hourly/HH
- <device root>/System/dhtHealth

Both measurements use {"tempF":number-or-null,"humidity":number-or-null}.
Both health payloads have identical fields and meanings. Shared source builds
the canonical suffix from topic_base_path; it is not separately configurable.

Gate also mirrors Env/tempHumidity and its hourly/HH topics for compatibility.
These are legacy aliases, not the primary layout. Car does not duplicate its
already canonical topic. MqttTopics.h retains the old literal MQTT_PUB_TEMP for
this alias only, preserving existing HA subscriptions. The previously supplied gatecounter-sensors.yaml was located and read. Its
gatecounter_temp and gatecounter_humidity definitions both subscribe to the old
path. Change only their state_topic to the canonical path; retain unique IDs,
names and device identifiers. This release does not erase retained topics.
MQTT Explorer may continue displaying the legacy branch and uses its own tree
ordering; equal canonical topic layout does not force equal screen positions.

To migrate HA later, change only Gate temperature/humidity state_topic from
msb/traffic/GateCounter/Env/tempHumidity to msb/traffic/GateCounter/Env/temp;
hourly consumers use the same replacement. Keep entity unique IDs/history intact.
Do not remove legacy aliases until all deployed consumers are verified migrated.

Both shared-service tests run with canonical and legacy configurations and check
primary publication, legacy measurement/hourly mirroring, raw health diagnostics,
null failures and corrected below-freezing measurements. Refactoring requires
matching primary schemas, not merely matching decoder helpers.

HA source: gatecounter-sensors.yaml, supplied Nov5. Both affected unique IDs
remain unchanged. Apply the two state_topic edits to the CURRENT HA file rather
than replacing newer unrelated settings from that older source. Reload MQTT
entities (or restart HA) after checking configuration. Canonical Gate publication
requires Gate .18. Compatibility aliases permit either update order.
