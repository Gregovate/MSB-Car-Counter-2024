# Nightly report contract
The four hour-ending labels 6PM through 9PM cover 5–9 PM local. After9PM includes every official passage from 9 PM through the device’s counting window, including closing allowance and dated extension. A completed zero is numeric. DailyTotal counts official passages for this date and ShowTotal is the season total. DailyAvgTemp is one show-window average or blank. Gate temperature is an unofficial backup.

ShowSummaryBuckets.csv contains period counts. ShowSummary.csv keeps Before5 and cumulative counts, with After9PM equal to DailyTotal. The existing MQTT summary retains its previous positions and optional extension-hour semantics.

On the first nightly write the firmware verifies a byte-for-byte .legacy copy of each old active report. The normal .previous file continues to protect replacement writes. Older bucket totals supply closing counts, including when the old 10PM cell was blank. Cumulative history uses the matching dated bucket DailyTotal. Without a verified daily total the new cumulative closing cells stay blank and the original values remain in .legacy. Previous seasons and -TEST archives retain their original schemas.
