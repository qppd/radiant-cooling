# API

> Communication interfaces for the Radiant Cooling System.
>
> Two data planes:
> 1. **ESP-NOW** — wireless link between the ESP32 boards (no Wi-Fi network
>    required between them).
> 2. **Firebase Realtime Database** — the cloud store shared by the gateway
>    board and the Flutter app.
>
> **Status: proposed draft** — will evolve as the firmware and app are
> implemented.

## 1. Overview

```
┌────────────────┐   ESP-NOW    ┌───────────────────────┐
│WaterChiller    │◄────────────►│RadiantCoolingMonitor  │
│Controller      │              │(gateway: Wi-Fi +      │
└────────────────┘              │ Firebase + ESP-NOW)   │
┌────────────────┐   ESP-NOW    │                       │
│Dehumidifier    │◄────────────►│                       │
│Controller      │              └───────────┬───────────┘
└────────────────┘                          │ HTTPS (REST)
                                            ▼
                                ┌───────────────────────┐
                                │Firebase Realtime      │
                                │Database               │
                                └───────────┬───────────┘
                                            │ Firebase SDK
                                            ▼
                                ┌───────────────────────┐
                                │RadiantCooling App     │
                                │(Flutter, Android)     │
                                └───────────────────────┘
```

- **Sensors → cloud:** monitor reads sensors, receives ESP-NOW packets from
  the two controllers, and writes everything to Firebase.
- **Cloud → plant:** the app writes commands/config to Firebase; the gateway
  streams those paths (real time) and forwards changes to the right board
  via ESP-NOW.

## 2. Device IDs

| Device ID  | Board / role                             |
| ---------- | ---------------------------------------- |
| `monitor`  | `RadiantCoolingMonitor` — **gateway**    |
| `chiller`  | `WaterChillerController` — ESP-NOW peer  |
| `dh`       | `DehumidifierController` — ESP-NOW peer  |

## 3. Firebase Realtime Database structure

Database URL shape: `https://<project>.firebaseio.com/`

```
radiant/
├── telemetry/
│   ├── monitor/                 # gateway's own sensors
│   │   └── latest               # { temps_c: [6 values] }
│   ├── chiller/
│   │   └── latest
│   └── dh/
│       └── latest
├── state/                       # current actuator states
│   ├── chiller/                 # { pump1, pump2 }
│   └── dh/                      # { dehumidifier }
├── config/                      # writable by app, consumed by firmware
│   ├── control/                 # gateway control params (chiller computation)
│   │   └── params               # { comfort_setpoint_c, dewpoint_margin_c, weather_cool_temp_c }
│   └── dh/                      # { humidity_setpoint_pct: 55, humidity_deadband_pct }
└── heartbeat/                   # gateway writes, app can watch
    └── monitor/                 # { online, firmware, ts }
```

### 3.1 `telemetry/<device>/latest`

`monitor` (6x DS18B20 + computed control values):

```json
{ "temps_c": [16.2, 16.8, 17.1, 22.4, 23.1, 22.8],
  "dew_point_c": 18.0, "water_floor_c": 20.0, "pumps": "on",
  "ts": 1786119829 }
```

`chiller` (1x DS18B20 + pump states):

```json
{ "water_temp_c": 16.2, "pump1": "on", "pump2": "off", "ts": 1786119829 }
```

`dh` (DHT22):

```json
{ "temp_c": 24.3, "humidity_pct": 52.0, "ts": 1786119829 }
```

> `ts` is stamped by the **gateway** (NTP time once synced, uptime-seconds
> as a fallback) when it forwards peer telemetry to Firebase — peers send
> the raw payload without a timestamp.

### 3.2 `state/<device>`

```json
{ "pump1": "on", "pump2": "off" }       // chiller
{ "dehumidifier": "on" }                 // dh
```

### 3.3 `config/<device>` (app → firmware)

```json
{ "comfort_setpoint_c": 24.0, "dewpoint_margin_c": 2.0, "weather_cool_temp_c": 28.0 }  // control (gateway)
{ "humidity_setpoint_pct": 55.0, "humidity_deadband_pct": 5.0 }                        // dh
```

| Config key              | Node      | Type   | Meaning                                       |
| ----------------------- | --------- | ------ | --------------------------------------------- |
| `comfort_setpoint_c`    | `control` | float  | Rooms above this → cooling demand (°C)        |
| `dewpoint_margin_c`     | `control` | float  | Water floor = dew point + margin (°C)         |
| `weather_cool_temp_c`   | `control` | float  | Outdoor temp above this → weather demand (°C) |
| `humidity_setpoint_pct` | `dh`      | float  | Dehumidifier target RH (%) — **55**           |
| `humidity_deadband_pct` | `dh`      | float  | RH hysteresis (%)                              |

## 4. Weather API (gateway → WeatherAPI.com)

The gateway fetches current outdoor conditions to drive the chiller
computation (see §6 Data flows):

- **Endpoint:** `GET https://api.weatherapi.com/v1/current.json?key=<KEY>&q=<LOCATION>`
- **Fields used:** `current.temp_c`, `current.humidity`, `current.dewpoint_c`
- **Throttle:** free tier has a daily call budget — keep the gateway's
  `WEATHER_POLL_S` conservative (default 900 s = 15 min).
- Credentials live in the gateway's `WEATHER_CONFIG.h` (git-ignored -
  copy from `WEATHER_CONFIG.example.h`).

## 5. ESP-NOW protocol

- **Built into the Arduino ESP32 core** (`esp_now.h`) — no external library.
- **Max payload:** 250 bytes (v1.0). Keep messages compact.
- **Channel lock:** all boards must operate on the same Wi-Fi channel as the
  gateway's router connection (ESP-NOW and station mode share the channel).
- Peer MAC addresses are registered on each board (see firmware sketches).

### 4.1 Message envelope (JSON, ≤ 250 B)

```json
{ "v": 1, "t": "telemetry", "src": "chiller", "seq": 42,
  "d": { "water_temp_c": 16.2, "pump1": "on", "pump2": "off" } }
```

| Field | Values                        | Meaning                     |
| ----- | ----------------------------- | --------------------------- |
| `v`   | int                           | Protocol version (starts at 1) |
| `t`   | `telemetry`, `state`, `cmd`, `config`, `status` | Message type |
| `src` | `monitor`, `chiller`, `dh`    | Sender device ID            |
| `seq` | unsigned int                  | Sequence number (dedupe)    |
| `d`   | object                        | Type-specific payload       |

> **Size note:** JSON keeps the protocol readable, but the full telemetry
> payload plus ArduinoJson overhead approaches the 250 B limit. If payloads
> grow, fall back to a packed binary struct (`__attribute__((packed))`).

### 4.2 Message types

| Type        | Direction                | Payload example                                      |
| ----------- | ------------------------ | ---------------------------------------------------- |
| `telemetry` | peer → gateway           | `{ "water_temp_c": 16.2, "pump1": "on", ... }`      |
| `state`     | peer → gateway           | `{ "pump1": "on", "pump2": "off" }`                |
| `cmd`       | gateway → peer           | `{ "cmd": "set_pumps", "value": "on" }`            |
| `config`    | gateway → peer           | `{ "comfort_setpoint_c": 24.0, "dewpoint_margin_c": 2.0 }` |
| `status`    | both                     | `{ "online": true, "firmware": "0.1.0" }`            |

### 4.3 Supported commands (`cmd`)

| Command                | Target  | Value           |
| ---------------------- | ------- | --------------- |
| `set_pumps`            | chiller | `"on"` / `"off"` |
| `set_humidity_target`  | dh      | number (%RH)    |
| `enable` / `disable`   | any     | -               |
| `reset`                | any     | -               |

## 6. Data flows

### Telemetry (sensors → app)
1. Peers read sensors, send `telemetry`/`state` to gateway over ESP-NOW.
2. Gateway (receive callback) pushes packets into a FreeRTOS queue.
3. Gateway `loop()` drains the queue, computes control values, and writes
   `telemetry/<device>/latest` to Firebase using the **FirebaseClient**
   library (async, non-blocking).

### Chiller control (computed on the gateway)
1. Gateway combines the WeatherAPI outdoor dew point, the indoor DHT22 dew
   point, water temperature (chiller telemetry) and room temperatures
   (6x DS18B20).
2. Reference dew point = **higher of outdoor/indoor**; water floor = dew
   point + `dewpoint_margin_c` (anti-condensation).
3. Pumps run when **weather demand** AND **sensor demand** AND water temp
   is safely above the floor (see `docs/diagrams/flow-chart.md`).
4. On a decision change, the gateway sends the `set_pumps` command to the
   chiller over ESP-NOW.

### Commands (app → plant)
1. App writes `config/<node>` in Firebase.
2. The gateway **streams** `radiant/config` in real time (FirebaseClient
   `stream()`) — changes arrive immediately, no polling.
3. On change, the gateway applies `config/control` locally or forwards a
   `cmd`/`config` ESP-NOW message to the right peer.
4. Peer applies the change and reports new `state` back.

## 7. Reliability & error handling

- **QoS:** ESP-NOW is best-effort; peers resend `telemetry`/`state` on the
  next cycle if `seq` gaps are detected by the gateway.
- **Retained state:** Firebase stores the latest state, so the app always
  shows the last known values.
- **Heartbeat:** the gateway writes `heartbeat/monitor` with `online` and
  `ts`; the app can show connectivity from this path.
- **Peer presence:** peers never touch Firebase directly — the app infers
  their online status from the `ts` of the latest `telemetry/<peer>/latest`
  write.
- **Unknown commands** are ignored and logged on the peer.

## 8. Firebase security rules & auth

Realtime Database **security rules** decide who can read/write each path.
Sensible defaults for this system:

| Path                    | Read            | Write           |
| ----------------------- | --------------- | --------------- |
| `radiant/telemetry/**`  | app             | gateway         |
| `radiant/state/**`      | app             | gateway         |
| `radiant/config/**`     | gateway         | app             |
| `radiant/heartbeat/**`  | app             | gateway         |

- The **gateway** authenticates via FirebaseClient — email/password or
  anonymous sign-in (the corresponding provider must be enabled in Firebase
  console → Auth). Credentials live in the gateway's `FIREBASE_CONFIG.h`
  (git-ignored - copy from `FIREBASE_CONFIG.example.h`).
- The **app** authenticates via the Firebase SDK (anonymous or Google sign-in
  for personal use).
- Rules are versioned alongside the data paths.

## 9. Versioning

- Firebase paths are versioned under `radiant/v1/` once stabilized.
- ESP-NOW payload fields are additive-only after v1; breaking changes bump
  the `v` field in the message envelope (see §5.1).
