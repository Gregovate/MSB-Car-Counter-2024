# MSB Car Counter 2024 Documentation

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

