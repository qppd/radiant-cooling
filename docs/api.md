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

Each installation also has a user-facing **system ID** (`SYSTEM_ID`, e.g.
`RADIANT-001`) in the gateway's `Config.h`. The gateway publishes it in the
**device registry** (`radiant/devices/<SYSTEM_ID>`) and in its heartbeat —
the Flutter app uses it to **link** to this specific system (see §6).

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
│   ├── weather/                 # app → gateway outdoor conditions (see §4)
│   └── dh/                      # { humidity_setpoint_pct: 55, humidity_deadband_pct }
├── heartbeat/                   # gateway writes, app can watch
│   └── monitor/                 # { online, device_id, firmware, ts }
└── devices/                     # device registry (system linking)
    └── <SYSTEM_ID>/             # { online, device_id, firmware, ts }
```

### 3.1 `telemetry/<device>/latest`

`monitor` (6x DS18B20 on the chilled-water pipes + computed control values;
`temps_c` = [supply, return, pipe1..pipe4]):

```json
{ "temps_c": [16.2, 16.8, 17.1, 22.4, 23.1, 22.8],
  "supply_c": 16.2, "return_c": 16.8, "coldest_pipe_c": 16.2,
  "delta_t_c": 0.6, "dew_point_c": 18.0, "water_floor_c": 20.0,
  "pumps": "on", "ts": 1786119829 }
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
| `comfort_setpoint_c`    | `control` | float  | Indoor air (DHT22) above this → cooling demand (°C) |
| `dewpoint_margin_c`     | `control` | float  | Water floor = dew point + margin (°C)         |
| `weather_cool_temp_c`   | `control` | float  | Outdoor temp above this → weather demand (°C) || `humidity_setpoint_pct` | `dh`      | float  | Dehumidifier target RH (%) — **55**          |
| `humidity_deadband_pct` | `dh`      | float  | RH hysteresis (%)                              |
| `weather` (node)        | `weather` | object | Outdoor conditions from the app — see §4     |

## 4. Weather API (Flutter app → Firebase → gateway)

The WeatherAPI.com **key is managed in the Flutter app**, never on the
ESP32. The app polls the current-weather endpoint and writes the result to
`radiant/config/weather`; the gateway streams that node like any other
config (see §6).

- **Endpoint (app):** `GET https://api.weatherapi.com/v1/current.json?key=<KEY>&q=<LOCATION>`
- **Fields used:** `current.temp_c`, `current.humidity`, `current.dewpoint_c`
- **Firebase path:** `radiant/config/weather` →
  `{ "temp_c": 31.2, "dewpoint_c": 24.5, "humidity_pct": 66.0, "ts": 1786119829 }`
- The gateway ignores the values once **stale** (older than
  `WEATHER_STALE_S`, default 3600 s = 1 h — e.g. the app is offline) and
  falls back to the **indoor** dew point only.
- **Throttle:** the free tier has a daily call budget — the app should poll
  conservatively (default 15 min, configurable in the app).
- The app stores the key in `lib/config/app_config.dart` (git-ignored —
  copy from `app_config.example.dart`).

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
1. The app polls WeatherAPI.com and writes the outdoor conditions to
   `radiant/config/weather`; the gateway streams them and combines the
   outdoor dew point, the indoor DHT22 temperature + humidity, the
   chilled-water supply/return + pipe temperatures (6x DS18B20) and the
   chiller tank temperature.
2. Reference dew point = **higher of outdoor/indoor**; water floor = dew
   point + `dewpoint_margin_c` (anti-condensation).
3. Pumps run when **weather demand** AND **sensor demand** (indoor DHT22
   temperature above setpoint) AND the **coldest** pipe/tank temperature
   is safely above the floor (see `docs/diagrams/flow-chart.md`).
4. On a decision change, the gateway sends the `set_pumps` command to the
   chiller over ESP-NOW.### Commands (app → plant)
0. **Linking:** on first run the app asks for the system ID; it validates
   it against the device registry (`radiant/devices/<SYSTEM_ID>`, written
   by the gateway) and stores it locally. From then on the app reads only
   that system's telemetry/state/heartbeat.
1. App writes `config/<node>` in Firebase.
2. The gateway **streams** `radiant/config` in real time (FirebaseClient
   `stream()` ) — changes arrive immediately, no polling.
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

| Path                    | Read                 | Write   |
| ----------------------- | -------------------- | -------- |
| `radiant/telemetry/**`  | app                  | gateway |
| `radiant/state/**`      | app                  | gateway |
| `radiant/config/**`     | app + gateway        | app     |
| `radiant/heartbeat/**`  | app                  | gateway |
| `radiant/devices/**`    | app                  | gateway |

**Ready-to-use rules:** [`firebase-security-rules.json`](firebase-security-rules.json) —
paste the file contents into Firebase console → Realtime Database → Rules
(or deploy via `firebase deploy --only database`).

- The **gateway** authenticates via FirebaseClient — **email/password with a
  dedicated account** (e.g. `gateway@radiant-cooling.local`). The rules above
  identify the gateway by `auth.token.email` (replace the placeholder email
  with the account you create), so **anonymous sign-in cannot be used for the
  gateway** with these rules — the writer role would be indistinguishable
  from the app.
- The **app** authenticates via the Firebase SDK (anonymous or Google sign-in
  for personal use); any authenticated user may read telemetry/state/config/heartbeat
  and write `config/**`.
- `config/**` is written by the app and read by both the app (dashboard
  display) and the gateway (stream).
- Rules are versioned alongside the data paths.

## 9. Versioning

- Firebase paths are versioned under `radiant/v1/` once stabilized.
- ESP-NOW payload fields are additive-only after v1; breaking changes bump
  the `v` field in the message envelope (see §5.1).
