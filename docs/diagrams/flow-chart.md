# Flow Charts

> Control logic flows for the ESP32 firmware — matches the implemented code
> in `src/esp/` (the pump decision lives in `ClimateControl.cpp`).

## 1. Main Control Loops

The three boards are split into two roles: the **gateway** (`RadiantCoolingMonitor`, the only board with Wi-Fi + Firebase) and the **ESP-NOW peers** (`WaterChillerController`, `DehumidifierController`).

### 1a. Gateway loop (`RadiantCoolingMonitor`)

```mermaid
flowchart TD
    A["Boot: load NVS settings, init sensors + ESP-NOW (pinned channel)"] --> B["Start Wi-Fi in the background<br/>(never blocks with saved credentials)"]
    B --> C["Read local DS18B20 sensors"]
    C --> D["Drain ESP-NOW queue (peer telemetry/state)"]
    D --> E{"Wi-Fi up AND weather fresh?"}
    E -->|Yes| F["Use outdoor weather<br/>(poll every WEATHER_POLL_S)"]
    E -->|No| G["Sensor-only: drop outdoor inputs,<br/>re-pin radio to ESP_NOW_CHANNEL"]
    F --> H["Compute dew point floor + pump decision"]
    G --> H
    H --> I{"Decision changed OR<br/>60 s resend due?"}
    I -->|Yes| J["Send set_pumps to chiller"]
    I -->|No| K{"Firebase ready?"}
    J --> K
    K -->|Yes| L["Write telemetry/heartbeat +<br/>process radiant/config stream"]
    K -->|No| M["Skip cloud writes - keep controlling locally"]
    L --> N["Offline: retry Wi-Fi every 60 s;<br/>Firebase backoff 60 s to 5 min"]
    M --> N
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
(`RadiantCoolingMonitor`) computes it from the available weather and all ESP
sensor readings, then sends a `set_pumps` command to the chiller. Weather is
optional: with no Wi-Fi/internet the gateway runs the same algorithm in
**sensor-only** mode. The chiller board only executes commands (with a local
fail-safe: pumps off if its water sensor is lost).

```mermaid
flowchart TD
    A["Inputs: outdoor dew point + temp<br/>(only if weatherValid), DHT22 temp + humidity,<br/>supply/return + pipe temps, chiller tank temp"]
    A --> B["Indoor dew point = Magnus(DHT22 temp, humidity)"]
    B --> C{"Weather valid?"}
    C -->|Yes| D["Ref dew point = max(outdoor, indoor)"]
    C -->|No| E["Ref dew point = indoor only"]
    D --> F["Water floor = ref dew point + margin"]
    E --> F
    F --> G{"Sensor demand?<br/>(indoor temp &gt; comfort setpoint)"}
    G -->|No| OFF["Pumps OFF"]
    G -->|Yes| H{"Dew point known?"}
    H -->|No| OFF
    H -->|Yes| I{"Weather demand?<br/>(outdoor temp &gt; weather_cool_temp;<br/>true when offline)"}
    I -->|No| OFF
    I -->|Yes| J{"Coldest pipe/tank above floor<br/>(+ hysteresis when starting)?"}
    J -->|No| OFF
    J -->|Yes| ON["Pumps ON"]
    ON --> K["Send set_pumps on change,<br/>then re-send every 60 s"]
    OFF --> K
```

> Thresholds (tunable via `config/control` in Firebase): comfort setpoint
> 24 °C, dew-point margin 2 °C, weather demand above 28 °C, hysteresis 1 °C.
> They are cached in NVS so an offline reboot keeps them. Hysteresis is
> applied by comparing against the floor ± margin depending on whether the
> pumps are already running. With no valid dew point (or no valid pipe/tank
> temperature) the decision is **OFF** (fail-safe).

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
- **Offline:** no WeatherAPI and no Firebase traffic. Pump control uses only
  the sensors; if the dew point is unavailable the pumps are held off. Peer
  telemetry still arrives over ESP-NOW, so the display/state stays live on
  the local gateway.
- **Fail-safe behaviour:** per-controller local guards sit below these flows
  (chiller: water-sensor loss → pumps off; dehumidifier: 3 failed DHT22
  reads → output off; gateway: unknown dew point/cold point → pumps off).
