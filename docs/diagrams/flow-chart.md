# Flow Charts

> Control logic flows for the ESP32 firmware — matches the implemented code
> in `src/esp/` (the pump decision lives in `ClimateControl.cpp`).

## 1. Main Control Loops

The three boards are split into two roles: the **gateway** (`RadiantCoolingMonitor`, the only board with Wi-Fi + Firebase) and the **ESP-NOW peers** (`WaterChillerController`, `DehumidifierController`).

### 1a. Gateway loop (`RadiantCoolingMonitor`)

```mermaid
flowchart TD
    A["Start"] --> B["Init sensors, ESP-NOW peers, Wi-Fi + Firebase"]
    B --> C["Read local 6x DS18B20 sensors"]
    C --> D["Drain ESP-NOW queue (peer telemetry)"]
    D --> E["Fetch weather (throttled: WEATHER_POLL_S)"]
    E --> F["Compute pump decision - weather + sensors + dew point"]
    F --> G{"Decision changed?"}
    G -->|Yes| H["Send set_pumps cmd to chiller over ESP-NOW"]
    G -->|No| I["Write telemetry + control values to Firebase"]
    H --> I
    I --> J["Stream: process radiant/config changes (realtime)"]
    J --> K{"Config changed?"}
    K -->|Yes| L["Apply locally or forward cmd/config to peer"]
    K -->|No| M["Reconnect Wi-Fi/Firebase if dropped"]
    L --> M
    M --> N["Publish heartbeat"]
    N --> C
```

### 1b. Peer loop (`WaterChillerController`, `DehumidifierController`)

```mermaid
flowchart TD
    A["Start"] --> B["Init hardware + ESP-NOW (register gateway)"]
    B --> C["Read sensors"]
    C --> D["Run control logic"]
    D --> E["Update outputs (SSR: pumps / dehumidifier)"]
    E --> F["Send telemetry/state to gateway over ESP-NOW"]
    F --> G{"ESP-NOW message received?"}
    G -->|cmd/config| H["Apply command / update configuration"]
    G -->|No| C
    H --> C
```

## 2. Water Pump Control Logic (computed on the gateway)

The pump decision is **not** made on the chiller board — the gateway
(`RadiantCoolingMonitor`) computes it from the WeatherAPI outdoor weather
(API key managed by the app) and all ESP sensor readings, then sends a
`set_pumps` command to the chiller. The chiller board only executes
commands (with a local fail-safe: pumps off if its water sensor is lost).

```mermaid
flowchart TD
    A["Inputs: outdoor dew point + temp (WeatherAPI), DHT22 temp + humidity, supply/return + pipe temps (DS18B20), chiller tank temp"]
    A --> B["Indoor dew point = Magnus(DHT22 temp, humidity)"]
    B --> C["Ref dew point = max(outdoor, indoor)"]
    C --> D["Water floor = ref dew point + margin"]
    D --> E{"Weather demand?<br/>(outdoor temp &gt; weather_cool_temp)"}
    E -->|Yes| F{"Sensor demand?<br/>(indoor DHT22 temp &gt; comfort setpoint)"}
    E -->|No| G["Pumps OFF"]
    F -->|Yes| H{"Coldest pipe/tank above floor?"}
    F -->|No| G
    H -->|Yes| I["Pumps ON"]
    H -->|No| G
    G --> J["Send set_pumps cmd to chiller (on change only)"]
    I --> J
```

> Draft thresholds (tunable via `config/control` in Firebase): comfort
> setpoint 24 °C, dew-point margin 2 °C, weather demand above 28 °C,
> hysteresis 1 °C. Hysteresis is applied by comparing against the floor
> ± margin depending on whether the pumps are already running.

## 3. Dehumidifier Control Logic

`DehumidifierController` — holds indoor relative humidity at the **55%**
setpoint (read from its own DHT22), with hysteresis. The humidity reading is
also sent to the gateway for the chiller dew-point computation.

```mermaid
flowchart TD
    A["Read DHT22 temperature + humidity"] --> B{"Humidity &gt; 55% + deadband?"}
    B -->|Yes| C["Dehumidifier ON (SSR)"]
    B -->|No| D{"Humidity &lt; 55% - deadband?"}
    D -->|Yes| E["Dehumidifier OFF"]
    D -->|No| F["Keep current state (hysteresis)"]
    C --> G["Send telemetry to gateway (temp, humidity, state)"]
    E --> G
    F --> G
```

> Setpoint 55 %RH, deadband 5 %RH by default (tunable via `config/dh`).

## Notes

- Hysteresis (deadbands) is applied to all switching logic to prevent SSR
  chatter and short-cycling of the pumps/dehumidifier.
- Fail-safe behaviour (e.g. pump override on sensor failure) is handled
  outside these flows and should be documented per-controller.
