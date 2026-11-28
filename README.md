# MSB Car Counter 2024 Documentation

Firmware in this checkout is **26.11.28.1**. Reverse and single-beam passages leave the idle arch pattern running. Daytime passages advance live daily and hourly MQTT counts and exercise detector feedback. HA-controlled daily resets and official show recording retain their separate windows. This counter downloads `EnterShowSummary.csv` and `EnterShowSummaryBuckets.csv`. Existing HTTP and SD paths are unchanged. The occupancy viewer explains how individual timestamped passages determine the hourly peak and identifies the counter reporting an archive error. File manager and report controls use Car red or Gate blue. Both report downloads and their HTML/print views convert existing legacy season rows on read. The original files remain available in the season folder.

Previous paired release: **26.11.25.1** (November 25, Central). Both devices reported the preceding Car .32 and Gate .25 builds installed. This release changes the embedded version only. Use YY.MM.DD.revision for official builds, starting at 1 each local release date. Increment for a distinct release on the same date. Reflashing the same binary keeps its version. The counters completed a year of testing. The first public production run is Friday, November 27, 2026.

**November 25 handoff for Firmware Refactoring Analysis Part 2.** Resume from `main` in all three repositories. Home Assistant owns the schedule, offsets and dated extension. Both counters consume those settings and return applied state, traffic observations and recording health. The November 24 source baseline was Car .31 / Gate .24. Production testing has been limited. Friday, November 27 is the first public-night field evaluation. November 24 telemetry confirmed both installed versions in TEST. Record Friday’s production observations in the linked system check.

[Show Controls integration and Friday checks](https://github.com/Gregovate/MSB-Show-Controls/blob/main/docs/friday-system-check.md) · [Peer firmware operating contract](https://github.com/Gregovate/MSB-Gate-Counter-2025/blob/main/docs/OPERATING_CONTRACT.md)

[Part 2 firmware release and Friday cutover](docs/Production_Cutover_20261124.md)

**Start here:** [Authoritative Car/Gate operating contract](docs/OPERATING_CONTRACT.md) — HA schedule, show-only logging, November 27 test-to-production cutover, Gate's 13-minute travel-time rationale, and implementation status. This overrides historical 24/7 logging notes.

## Overview
The MSB Car Counter 2024 is a sophisticated vehicle traffic monitoring system designed to manage park capacity and traffic flow. It uses an ESP32 microcontroller paired with sensors and advanced software to detect, log, and publish real-time vehicle data.

---

## Features

### Vehicle Detection
- **Beam Sensors**: Accurate vehicle detection using IR beam sensors.
- **State Machine Logic**: Ensures reliable detection and eliminates false positives.

### Data Management
- **Real-Time Logging**: Logs hourly, daily, and show-specific vehicle counts to an SD card.
- **Data Recovery**: Retains counts and settings across reboots for persistence.

### Environmental Monitoring
- **Temperature and Humidity**: Records and publishes environmental data using a DHT22 sensor.

### Networking and Integration
- **MQTT Protocol**: Publishes vehicle counts, environmental data, and status updates to an MQTT server.
- **WiFi Support**: Connects to multiple WiFi networks via WiFiMulti for seamless operation.
- **OTA Updates**: Enables firmware updates over the air with ElegantOTA.

### Real-Time Display
- **OLED Display**: Displays live vehicle counts, temperature, and system status.

---

## Setup Instructions

### Hardware Requirements
- ESP32 (DOIT DevKit V1 recommended)
- DHT22 sensor
- IR beam sensors
- OLED display (128x64 resolution)
- SD card module

### Software Requirements
- Arduino IDE with required libraries:
  - `ArduinoJson`, `PubSubClient`, `Adafruit_GFX`, `Adafruit_SSD1306`, `RTClib`, `DHT`, `NTPClient`

### Configuration
- Update `secrets.h` with WiFi credentials.
- Configure MQTT topics and server details.

### Deployment
- Upload the firmware using Arduino IDE.
- Monitor the system via Serial Monitor for debugging.

---

## Key Metrics
- **Hourly and Daily Vehicle Counts**
- **Show-Specific Counts and Summaries**
- **Environmental Data (Temperature and Humidity)**

---
## Documentation

For more detailed information about specific components, see the following:

- [MQTT Reset Topics](docs/mqtt_reset_topics.md): Detailed descriptions of MQTT reset topics and their payloads.
- [Changelog](docs/changelog.md): A history of updates, fixes, and new features in the Gate Counter project.

---

## Contact
**Greg Liebig**  
Email: `greg@engrinnovations.com`

