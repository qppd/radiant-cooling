# API

> Communication interfaces for the Radiant Cooling System.
>
> Two data planes:
> 1. **ESP-NOW** — wireless link between the ESP32 boards (no Wi-Fi network
>    required between them).
> 2. **Firebase Realtime Database** — the cloud store shared by the gateway
>    board and the Flutter app.
>
> **Status: implemented** — matches the firmware in `src/esp/` and the
> Flutter app in `src/app/`; details evolve with the code.

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
- **Cloud is optional:** the ESP-NOW mesh and the gateway's pump control run
  with no router or internet. Weather + Firebase are a *bonus* path; see
  §4 (weather) and §6/§7 (control and failure behaviour).

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
│   │   └── latest               # { temps_c: [4 values] }
│   ├── chiller/
│   │   └── latest
│   ├── dh/
│   │   └── latest
│   └── weather/
│       └── latest               # gateway's WeatherAPI reading (see §4)
├── state/                       # current actuator states
│   ├── chiller/                 # { compressor, condenser_fan, outgoing_pump }
│   └── dh/                      # { dehumidifier }
├── config/                      # writable by app, consumed by firmware
│   ├── control/                 # gateway control params (chiller computation)
│   │   └── params               # { comfort_setpoint_c, dewpoint_margin_c, weather_cool_temp_c }
│   ├── weather_key/             # WeatherAPI key, app → gateway (see §4)
│   └── dh/                      # { humidity_setpoint_pct: 55, humidity_deadband_pct }
├── heartbeat/                   # gateway writes, app can watch
│   └── monitor/                 # { online, device_id, firmware, ts }
└── devices/                     # device registry (system linking)
    ├── <SYSTEM_ID>/             # { online, device_id, firmware, ts }
    ├── chiller/                 # { online, role: 'peer', parent, ts }
    └── dh/                      # { online, role: 'peer', parent, ts }
```

The `chiller` and `dh` entries are liveness records only: peers never touch
Firebase, so the **app watchdog** creates them and flips `online` to `false`
when that device stops reporting (and back to `true` when it resumes). They
carry `role: 'peer'`, so they are not offered as linkable systems.

### 3.1 `telemetry/<device>/latest`

`monitor` (4x ceiling DS18B20, one 1-Wire bus each + computed control
values; `temps_c` = [ceiling1..ceiling4], where sensors 1-2 feed
supply/return):

```json
{ "temps_c": [16.2, 16.8, 17.1, 22.4],
  "supply_c": 16.2, "return_c": 16.8, "coldest_pipe_c": 16.2,
  "delta_t_c": 0.6, "outdoor_temp_c": 31.2, "outdoor_dewpoint_c": 24.5,
  "outdoor_humidity_pct": 66.0, "weather_valid": true,
  "control_source": "weather+sensors", "dew_point_c": 18.0,
  "water_floor_c": 20.0, "pumps": "on", "compressor": "on",
  "condenser_fan": "on", "outgoing_pump": "off", "ts": 1786119829 }
```

> With no usable weather the `outdoor_*` fields are the `-100` invalid
> sentinel, `weather_valid` is `false`, and `control_source` is
> `sensors_only` (or `failsafe` when the dew point is unknown).

`chiller` (two DS18B20 tank sensors + compressor and pump states;
`water_temp_c` = coldest valid of outgoing/ingoing):

```json
{ "outgoing_temp_c": 16.2, "ingoing_temp_c": 16.8,
  "water_temp_c": 16.2, "compressor": "on", "condenser_fan": "on",
  "pump1": "on", "outgoing_pump": "on", "ts": 1786119829 }
```

`dh` (DHT22):

```json
{ "temp_c": 24.3, "humidity_pct": 52.0, "ts": 1786119829 }
```

`weather` (written by the gateway right after each WeatherAPI fetch, so the
app does not have to wait for the 5 s monitor cycle):

```json
{ "location": "Manila", "temp_c": 31.2, "feels_like_c": 36.0,
  "humidity_pct": 66.0, "dew_point_c": 24.5, "wind_kph": 12.5,
  "condition": "Partly cloudy", "is_day": 1,
  "obs_epoch": 1786119000, "ts": 1786119829 }
```

> `ts` is stamped by the **gateway** (NTP time once synced, uptime-seconds
> as a fallback) when it forwards peer telemetry to Firebase — peers send
> the raw payload without a timestamp.

### 3.2 `state/<device>`

```json
{ "compressor": "on", "condenser_fan": "on",
  "pump1": "on", "outgoing_pump": "on" }       // chiller
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
| `weather_cool_temp_c`   | `control` | float  | Outdoor temp above this → weather demand (°C); ignored offline |
| `humidity_setpoint_pct` | `dh`      | float  | Dehumidifier target RH (%) — **55**            |
| `humidity_deadband_pct` | `dh`      | float  | RH hysteresis (%)                              |
| `weather_key` (node)    | `weather_key` | string | WeatherAPI key delivered to the gateway — see §4 |

### 3.4 `devices/<SYSTEM_ID>/control` (app -> gateway)

The dashboard writes this node; the gateway streams it for immediate
response. `mode` selects auto or manual. The two output keys are acted on
**only in manual mode**; auto mode uses the computed `set_pumps` command.

```json
{ "mode": "manual", "compressor": "on", "outgoing_pump": "off" }
```

| Key | Type | Meaning |
| --- | --- | --- |
| `mode` | string | `auto` or `manual` |
| `compressor` | string | Manual SSR1: compressor + condenser fan + chiller pump (`on`/`off`) |
| `outgoing_pump` | string | Manual SSR2: outgoing pump (`on`/`off`) |
| `pumps` | string | Legacy combined command; sets both outputs |

The app writes `mode`, `compressor` and `outgoing_pump` together when it
enters manual mode, seeded with the chiller's current state, so an old manual
command cannot start the compressor unexpectedly. The gateway re-sends the
two manual states every 60 s (and after a chiller reboot). In auto mode the
gateway drives both outputs together with `set_pumps`.

## 4. Weather API (monitor board fetches; key managed by the app)

The **monitor (gateway) board** fetches the current outdoor conditions
itself from WeatherAPI.com. The API key is **managed by the Flutter app**
and delivered to the gateway at runtime — it is never compiled into the
firmware.

- **Endpoint (gateway):** `GET https://api.weatherapi.com/v1/current.json?key=<KEY>&q=<LOCATION>&aqi=no`
- **Fields used:** `current.temp_c`, `current.humidity`, `current.dewpoint_c`
  for control, plus `feelslike_c`, `wind_kph`, `condition.text`, `is_day` and
  `last_updated_epoch` for the app's weather card.
- **Published:** the reading is written to `radiant/telemetry/weather/latest`
  (and the `outdoor_*` fields of `telemetry/monitor/latest` used by control).
- **Key delivery:** the app writes the key to `radiant/config/weather_key`
  (a string); the gateway streams it and keeps it in RAM. A gateway reboot
  loses it, so the app re-sends it on launch / (re)link. The gateway does not
  rely on the stream alone: it also reads the key back over REST every
  `WEATHER_KEY_RETRY_S` (60 s) while it has none, refreshes it every
  `WEATHER_KEY_REFRESH_S` (1 h) to catch a rotation, and re-reads immediately
  after a failed fetch — so a dropped stream or a key set while the gateway
  was offline still reaches it.
- **Throttle:** the gateway polls every `WEATHER_POLL_S` (default 900 s =
  15 min) to stay inside the free-tier daily call budget.
- **Wi-Fi loss:** the weather snapshot is dropped immediately (no fetch is
  attempted) and the gateway switches to **sensor-only** control. On
  reconnect it fetches weather as soon as the link is up.
- **Failure handling (Wi-Fi up, API failing):** the last good values are kept
  for up to `WEATHER_STALE_S` (default 3600 s = 1 h); after that they are
  discarded and control uses the **indoor** dew point only. `outdoor_*`
  telemetry fields are the `-100` invalid sentinel while there is no usable
  reading.
- The app stores the key in `lib/config/app_config.dart` (git-ignored —
  copy from `app_config.example.dart`) or enters it in the app.
- No Firebase access happens while offline: the config stream/telemetry writes
  are skipped, and the cloud task reconnects Firebase with backoff (1 min →
  5 min) once Wi-Fi returns.

## 5. ESP-NOW protocol

- **Built into the Arduino ESP32 core** (`esp_now.h`) — no external library.
- **Max payload:** 250 bytes (v1.0). Keep messages compact.
- **Channel lock:** all boards must operate on the same Wi-Fi channel as the
  gateway's router connection (ESP-NOW and station mode share the channel).
- Peer MAC addresses are registered on each board (see firmware sketches).

### 5.1 Message envelope (JSON, ≤ 250 B)

```json
{ "v": 1, "t": "telemetry", "src": "chiller", "seq": 42,
  "d": { "water_temp_c": 16.2, "pump1": "on" } }
```

| Field | Values                        | Meaning                     |
| ----- | ----------------------------- | --------------------------- |
| `v`   | int                           | Protocol version (starts at 1) |
| `t`   | int (`1`=telemetry, `2`=state, `3`=cmd, `4`=config, `5`=status) | Message type |
| `src` | `monitor`, `chiller`, `dh`    | Sender device ID            |
| `seq` | unsigned int                  | Sequence number (dedupe)    |
| `d`   | object                        | Type-specific payload       |

> **Size note:** JSON keeps the protocol readable, but the full telemetry
> payload plus ArduinoJson overhead approaches the 250 B limit. If payloads
> grow, fall back to a packed binary struct (`__attribute__((packed))`).

### 5.2 Message types

| Type        | Direction                | Payload example                                      |
| ----------- | ------------------------ | ---------------------------------------------------- |
| `telemetry` | peer → gateway           | `{ "water_temp_c": 16.2, "compressor": "on", "outgoing_pump": "on", ... }`      |
| `state`     | peer → gateway           | `{ "compressor": "on", "condenser_fan": "on", "outgoing_pump": "on" }`            |
| `cmd`       | gateway → peer           | `{ "cmd": "set_pumps", "value": "on" }`            |
| `config`    | gateway → peer           | `{ "humidity_setpoint_pct": 55.0, "humidity_deadband_pct": 5.0 }` |
| `status`    | both                     | `{ "online": true, "firmware": "0.1.0" }`            |

### 5.3 Supported commands (`cmd`)

| Command                | Target  | Value           |
| ---------------------- | ------- | --------------- |
| `set_pumps`            | chiller | `"on"` / `"off"` (both outputs) |
| `set_compressor`       | chiller | `"on"` / `"off"` (SSR1, manual) |
| `set_outgoing_pump`    | chiller | `"on"` / `"off"` (SSR2, manual) |
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
1. **Online:** the gateway polls WeatherAPI.com (key delivered by the app via
   `radiant/config/weather_key`) and combines the outdoor dew point, the
   indoor DHT22 temperature + humidity, the 4x ceiling pipe temperatures
   (DS18B20) and the chiller tank water temperature (coldest of the
   outgoing/ingoing sensors).
2. Reference dew point = **higher of outdoor/indoor**; water floor = dew
   point + `dewpoint_margin_c` (anti-condensation).
3. **Offline (sensor-only):** there is no outdoor reading, so weather demand
   is treated as satisfied. Demand comes from the indoor DHT22 temperature
   above `comfort_setpoint_c`, and the reference dew point is the **indoor**
   dew point only. With no valid indoor humidity (or no valid pipe/tank
   temperature) the pumps are held **off**.
4. Pumps run when demand is present AND the **coldest** pipe/tank
   temperature is safely above the water floor (hysteresis applies;
   see `docs/diagrams/flow-chart.md`).
5. In **auto** mode, on a decision change the gateway sends `set_pumps`
   (both outputs) to the chiller over ESP-NOW. In **manual** mode it forwards
   `set_compressor` (SSR1: compressor + condenser fan + chiller pump) and
   `set_outgoing_pump` (SSR2) independently. Either path **re-sends the
   current command every 60 s** (`PUMP_CMD_RESEND_S`) so a lost frame or a
   peer reboot self-heals. After a gateway reboot the output state is adopted
   from the chiller's telemetry and the cached DH config is re-sent.
6. Control thresholds/DH setpoints are cached in NVS and survive an offline
   power cycle; the gateway always boots in **auto** mode.

### Commands (app → plant)
0. **Linking:** on first login (new account, or any signed-in user with no
   linked system) the app shows a dedicated linking page asking for the
   system ID; it validates it against the device registry
   (`radiant/devices/<SYSTEM_ID>`, written by the gateway) and stores it
   locally before the dashboard is shown. From then on the app reads only
   that system's telemetry/state/heartbeat.
1. App writes `config/<node>` in Firebase.
2. The gateway **streams** `radiant/config` in real time (FirebaseClient
   `stream()` ) — changes arrive immediately, no polling.
3. On change, the gateway applies `config/control` locally or forwards a
   `cmd`/`config` ESP-NOW message to the right peer.
4. Peer applies the change and reports new `state` back.

## 7. Reliability & error handling

- **QoS:** ESP-NOW is best-effort with no ACK/retry in the firmware.
  Peers publish fresh `telemetry`/`state` on a fixed cycle, and the gateway
  forwards the latest snapshot to Firebase — a lost packet is simply
  overwritten by the next cycle. The `seq` field exists for future gap
  detection/deduplication.
- **Retained state:** Firebase stores the latest state, so the app always
  shows the last known values.
- **Heartbeat:** the gateway writes `heartbeat/monitor` with `online` and
  `ts`; the app can show connectivity from this path.
- **Peer presence:** peers never touch Firebase directly — their online
  status can be inferred from the freshness of `telemetry/<peer>/latest`.
  The app currently surfaces the gateway heartbeat; peer data appears on
  the dashboard as soon as a peer reports.
- **Offline operation:** the gateway boots and controls with **no Wi-Fi at
  all** (or Wi-Fi without internet). Saved-network reconnects run every
  60 s in the background; the board never restarts on a network failure. The
  radio is pinned to `ESP_NOW_CHANNEL` while the AP is unreachable. On
  reconnect the mesh is already running, Firebase re-initialises with
  60 s → 5 min backoff, and telemetry/config resumes.
- **State resync without the cloud:** the gateway re-sends the cached DH
  config when the DH peer reports `status`, adopts the chiller's reported
  output state after a gateway reboot, and periodically re-sends `set_pumps`
  (auto) or the two manual commands.
- **Command idempotence:** `set_pumps` is idempotent, so the 60 s resend is
  safe; duplicate `config`/`cmd` frames are harmless.
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
- The **app** authenticates via the Firebase SDK with **email/password**
  (login and signup screens in the app); any authenticated user may read
  telemetry/state/config/heartbeat and write `config/**`.
- `config/**` is written by the app and read by both the app (dashboard
  display) and the gateway (stream).
- `config/weather_key` is special-cased: written by the app, readable only
  by the gateway (it holds the WeatherAPI key).
- Rules are versioned alongside the data paths.

## 9. Versioning

- Firebase paths are versioned under `radiant/v1/` once stabilized.
- ESP-NOW payload fields are additive-only after v1; breaking changes bump
  the `v` field in the message envelope (see §5.1).
