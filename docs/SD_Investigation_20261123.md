# SD recovery investigation — November 23, 2026

Scope: Car .17 / Gate .13 currently deployed. Source-only corrective candidates Car .18 / Gate .14. No device update or licensed ESP32 build performed for these candidates.

## Verified evidence
Both cards mounted but serial subsequently reported failed CMD17 reads (0x11) and CMD13 status requests (0x0d). The full original-card backup contains valid 18/17 daily checkpoint records, show total 18, valid latest hourly CSV and /data UI files. The photographed daily count 19 is absent from that backup. Both cards later produced readable files; hardware failure, startup power instability, and firmware interaction remain possible. No measured voltage transient or deterministic reproduction of low-level driver failure exists.

The archived 2025 main.cpp uses CS GPIO5 and SD.begin(PIN_SPI_CS, SPI, 4000000), the same as .17. Its web server also streams SD files asynchronously. Therefore neither a changed SPI pin/frequency nor the mere existence of asynchronous reads is established as a new cause. New checkpoints, readback validation, calendar/settings copies and export renames increase activity, but correlation is not causation.

## Reproduced defects and targeted source changes
A real CountPersistence.cpp host test confirms that a startup missing SD sets retryClockRestore=false and never retries restoration when the mock card becomes available later. Candidate changes permit the existing rate-limited restoration service to retry a missing-card dependency. After a valid restore, countReady prevents replay of the saved snapshot over live totals. Invalid/corrupt checkpoints remain fail-closed: they are NOT blindly retried, discarded or replaced. This is not an automatic SD remount: an unmounted Arduino SD instance still needs an independently safe reinitialization design.

The normal display redraw omitted runtime SD availability. Candidate .18/.14 overlays SD FAULT / COUNTS MAY BE UNSAVED after normal content, keeping counts below the warning. Actual display source is compiled against a recording mock and checked across repeated redraws and restored health. COUNT RECOVERY remains when no snapshot has restored.

## Remaining work — not closed by these candidates
* Low-level SD driver failure is unexplained; do not claim a defective card/reader or a confirmed firmware root cause.
* Missing-card retry does not solve a mounted card with unreadable/corrupt checkpoints, power-off loss, or all transient read failures.
* The existing NVS pending vehicle total has no per-event date/hour or automatic reconciliation. Complete durable event journal and replay are NOT implemented here.
* countPersistence may retain a prior OK for unchanged totals; SD health/status/export remain separate observations. A unified health model remains required.
* SD.end/remount cannot be introduced safely while asynchronous web responses hold open SD files. File ownership/serialization and counting continuity need design and fault tests first.
* No claim of nonblocking detection: foreground SD/network work remains synchronous.
* No physical OLED/controller driver change; installed Car screen is labelled SSD1309 while code uses SSD1306 API. Initialization/corruption is not solved by the overlay.

The 2024 drawings are historical; do not rewire present detectors from them. Preserve running healthy counts during the show. These source candidates require a licensed local build and controlled field verification before being called deployed or operationally accepted.
