## MQTT Reset Topics

### Reset Topics

#### `msb/traffic/CarCounter/resetDailyCount`
**Purpose**: Resets the daily car count.

- **Payload**: Integer value representing the new daily count.
- **Example**:
  ```json
  {"payload": 0}
  ```
  Resets the daily count to `0`.

#### `msb/traffic/CarCounter/resetShowCount`
**Purpose**: Resets the show-specific car count.

- **Payload**: Integer value representing the new show count.
- **Example**:
  ```json
  {"payload": 0}
  ```
  Resets the show count to `0`.

#### `msb/traffic/CarCounter/resetDayOfMonth`
**Purpose**: Updates the internal calendar to a specific day of the month.

- **Payload**: Integer value (1–31) representing the desired day.
- **Example**:
  ```json
  {"payload": 15}
  ```
  Sets the day of the month to `15`.

#### `msb/traffic/CarCounter/resetDaysRunning`
**Purpose**: Resets the number of days the show has been running.

- **Payload**: Integer value representing the new "days running" count.
- **Example**:
  ```json
  {"payload": 1}
  ```
  Resets the days running count to `1`.

#### `msb/traffic/CarCounter/carCounterTimeout`
**Purpose**: Updates the timeout duration for car counter detection.

- **Payload**: Integer value (in milliseconds) for the timeout duration.
- **Example**:
  ```json
  {"payload": 60000}
  ```
  Sets the timeout to `60,000 ms` (1 minute).

#### `msb/traffic/CarCounter/waitDuration`
**Purpose**: Adjusts the wait duration for car detection between sensors.

- **Payload**: Integer value (in milliseconds) for the wait duration.
- **Example**:
  ```json
  {"payload": 950}
  ```
  Sets the wait duration to `950 ms`.

#### `msb/traffic/CarCounter/resetHourlyCounts`
**Purpose**: Resets all hourly car counts for the current day.

- **Payload**: None required.
- **Example**:
  ```json
  {}
  ```
  Resets all hourly car counts to `0`.

#### `msb/traffic/CarCounter/resetDailySummary`
**Purpose**: Clears the daily show summary data.

- **Payload**: None required.
- **Example**:
  ```json
  {}
  ```
  Resets the daily summary file.

#### `msb/traffic/CarCounter/resetTemperatureAverages`
**Purpose**: Resets hourly temperature averages for the current day.

- **Payload**: None required.
- **Example**:
  ```json
  {}
  ```
  Clears all recorded hourly temperature averages.

#### `msb/traffic/CarCounter/reboot`
**Purpose**: Reboots the ESP32 device.

- **Payload**: None required.
- **Example**:
  ```json
  {}
  ```
  Initiates a system reboot.