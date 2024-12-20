## Changelog

### [1.3.0] - 2024-12-19
#### Added
- Published hourly average temperatures via MQTT.
- Improved temperature and humidity logging logic for accurate summaries.
- Enhanced state machine logic for vehicle detection reliability.

#### Fixed
- Resolved issue with average show temperature always being zero.
- Corrected redundant declaration of `hourlyTemp` array in `averageHourlyTemp()` function.

#### Changed
- Refactored `saveDailyShowSummary()` for better handling of hourly temperature calculations.
- Streamlined MQTT topic naming for temperature and hourly data.

### [1.2.0] - 2024-12-10
#### Added
- Integrated DHT22 sensor for temperature and humidity monitoring.
- Hourly temperature averages calculated and stored for summaries.
- SD card functionality to log vehicle counts and environmental data.

#### Fixed
- Resolved MQTT connection stability issues with improved reconnect logic.
- Debugged data recovery process during reboots to prevent count loss.

#### Changed
- Updated MQTT topics to support environmental data.
- Improved logging for better debugging and monitoring.

### [1.1.0] - 2024-12-01
#### Added
- Implemented ElegantOTA for firmware updates.
- Introduced WiFiMulti for seamless multi-network connection support.
- Added OLED display support for real-time monitoring of counts and temperature.

#### Fixed
- Addressed sensor bounce issues during detection in snowy conditions.
- Resolved SD card initialization failure for specific configurations.

#### Changed
- Optimized state machine logic for car detection.

### [1.0.0] - 2024-11-15
#### Added
- Initial implementation of car counter using ESP32.
- Vehicle detection with IR beam sensors.
- MQTT integration for remote monitoring of vehicle counts.
- SD card support for local data logging.
- Time synchronization using NTP and RTC.
- Basic OLED display support.

---

## Future Enhancements
- Add a web-based dashboard for real-time data visualization.
- Integration of additional environmental sensors for advanced analytics.
- Enhanced power efficiency for long-term, battery-powered deployments.

---

This changelog follows semantic versioning. For further details, please contact:
**Greg Liebig**  
Email: `greg@engrinnovations.com`
