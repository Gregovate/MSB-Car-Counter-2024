# Official outdoor report temperature — 2026-11-24

Car .28 / Gate .21. Operator clarified that Car's exterior DHT22 is the official
outdoor source; Gate's sensor is inside the metal gate/battery enclosure and is
an unofficial backup. No cross-device substitution or offset is introduced.
Report identity text now states that distinction. CSV schemas remain unchanged.

Investigation found that accepted official-window sample sums/counts existed only
in RAM. Reboot/OTA lost them. A subsequent same-date summary rewrite could replace
a previously exported average with an empty field. Nov23 also had invalid DHT
measurements before the negative-encoding correction; measurements never accepted
or saved cannot be reconstructed from a current temperature or aggregate car count.
The screenshots prove blanks in saved rows, not a hidden table column.

Car now saves the complete per-date hourly/show sample sums and counts in NVS
(namespace traffic-env, alternating a/b blobs). Each includes magic, monotonic
sequence, full local date/hour and checksum; writes are read back and compared.
The newest valid slot restores weighted averages on reboot, including a real
zero F. Incomplete/corrupt newest state falls back to the other complete slot.
Snapshots are saved once per accepted minute sample, independently of SD/MQTT.
Existing IRAM beam capture protects acquisition during flash/cache stalls. No
journal/count record layout, partition table, OTA slot or count reset changes.
Gate does not write this official-temperature checkpoint; its readings remain
RAM-only backup measurements. Common source remains identical in both repos.

Full-date/hour guards prevent mixing future saved samples after clock rollback.
The saved state is preserved and restored automatically once the clock catches
up. New dates start fresh daily/show totals; yesterday's average cannot become
today's. dhtHealth.samplePersistence reports OK, RESTORED, NEW_DAY, failure states,
or BACKUP_SENSOR_RAM_ONLY for Gate. Current sensor validity is still independent
of a restored historical average.

If no current show average exists, the same-date valid DailyAvgTemp may be kept
from the device's current or .previous cumulative/bucket CSV. Date, column position
and finite -40..120 F range must match. No current live reading, Gate temperature,
or guessed historical value fills a missing night. Reports that have no surviving
temperature evidence remain blank. Existing count columns are unaffected.

Today's daytime vehicle tests continue in 24/7 daily traffic. Official show
counts and temperature sampling use the accepted HA show window. For tonight's
simulated show, check that the final DailyAvgTemp is populated; a reboot afterward
must retain that measured average. User builds/flashes locally as usual; no SD UI
upload or filesystem flash is needed. The new checkpoint starts with installation
and cannot retroactively recreate missing samples from the earlier builds.
