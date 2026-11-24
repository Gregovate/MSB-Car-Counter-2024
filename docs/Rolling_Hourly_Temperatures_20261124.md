# Rolling hourly temperature MQTT — 2026-11-24

Car .29 / Gate .22 restore retained hourly summaries in the same common module.
Previous shared code sent completed-hour averages without retention, only at hour
closure. Gate's legacy tempHumidity/hourly slots could still show older retained
values with no date, whereas a fresh MQTT subscription to Car showed no history.
Those old undated values cannot be identified as current-day measurements.

Operator clarified that the desired display is rolling hours, not a current-day
only array. Both devices now expose the last 24 COMPLETED hours:
- <device root>/Env/temp/hourly/HH = numeric Fahrenheit average (retained).
- <device root>/Env/temp/hourlyDate/HH = YYYYMMDD for that bucket (retained).

The current unfinished hour is not a finalized summary. That HH slot can contain
its previous-day completed bucket until the current hour closes. For example,
at 10:30, slot 10 can be yesterday's 10:00–11:00, while slot 09 is today's 09:00–10:00.
Its date topic removes ambiguity. Buckets expire 24 hours after their end time.
Unknown/expired buckets are removed using empty retained payloads, not fake zero F.

At midnight the rolling history keeps the prior day's completed buckets. Daily
and official-show sample averages still reset by full local date and remain
separate from rolling hourly presentation. MQTT reconnect and periodic environment
publication republish the dated rolling snapshot. The canonical layout is the
same on Car/Gate. Gate also mirrors legacy Env/tempHumidity/hourly/HH and the
corresponding hourlyDate/HH metadata for existing subscribers.

Car's completed buckets use fixed NVS keys h00..h23 in traffic-env; each stores
magic, date, hour, float average and checksum, with write/read verification.
Failed slot writes remain dirty and retry on publication while the bucket is
relevant. This adds no files or partition changes and does not alter the .28 a/b
official-average checkpoint schema. Existing completed .28 samples seed rolling
slots, including the prior night's unclosed hour after a midnight reboot.
No old broker value with an unknown date is guessed into the saved history.
Gate remains an unofficial RAM-only backup, so reboot can legitimately leave
its rolling history incomplete. Car is the official outdoor source.

Tests cover retained delivery, per-hour dates, midnight continuity, replay after
cold restore, migration from the .28 sample checkpoint, 24-hour expiry, no invented
zero measurements, and unchanged official daily-average persistence. A new
installation has only the actual available history; the next hour close supplies
its first new completed bucket. Daytime tests and accepted show-window behavior
continue unchanged. Build/flash locally as usual, no SD webpage upload needed.
