# ESP32 Firmware

Three standalone Arduino sketches. **Every component type and every external
library is encapsulated in its own class module** — the `.ino` files are glue
only (wiring + control logic).

## Board folders

| Folder                     | Hardware                                                     | Role     |
| -------------------------- | ------------------------------------------------------------ | -------- |
| `RadiantCoolingMonitor/`   | 4x DS18B20 ceiling sensors (one 1-Wire bus each: 23/21/18/5) + WiFi reset button (25) | Gateway  |
| `WaterChillerController/`  | 2x DS18B20 tank sensors (out=5, in=18), compressor SSR (23), pump relay (19) | Peer     |
| `DehumidifierController/`  | DHT22 (GPIO25), 1x SSR (GPIO23) -> dehumidifier          | Peer     |

## Module layout (same pattern in every sketch folder)

```
<BoardName>/
├── <BoardName>.ino     # glue: instantiate components, setup()/loop(), control logic
├── Config.h            # board config (MACs, SYSTEM_ID, constants); includes PINS_CONFIG.h + FirebaseConfig.h
├── PINS_CONFIG.h       # pin assignments - all GPIO wiring for the board
├── FirebaseConfig.h    # Firebase class declaration (endpoints + path layout) [gateway only]
├── FirebaseConfig.cpp  # REAL Firebase credentials (GIT-IGNORED; copy from FirebaseConfig.cpp.example) [gateway only]
├── TemperatureSensor.{h,cpp}   # component: DS18B20   (wraps OneWire + DallasTemperature)
├── HumiditySensor.{h,cpp}      # component: DHT22     (wraps DHT)
├── SsrOutput.{h,cpp}           # component: SSR output (wraps Arduino digital I/O)
├── WifiProvisioner.{h,cpp}     # wifi:     WiFiManager branded captive portal + auto-reconnect (wraps WiFiManager) [gateway only]
├── EspNowTransport.{h,cpp}     # comms:    ESP-NOW    (wraps WiFi + esp_now)
├── JsonProtocol.{h,cpp}        # protocol: JSON envelope (wraps ArduinoJson)
├── FirebaseSync.{h,cpp}        # cloud:    Firebase RTDB set/update + realtime stream (wraps FirebaseClient) [gateway only]
├── WeatherApi.{h,cpp}          # cloud:    WeatherAPI.com current weather (key from the app) [gateway only]
└── ClimateControl.{h,cpp}      # control:  dew point + pump decision [gateway only]
```

Pin assignments are kept in `PINS_CONFIG.h` (hardware wiring) and everything
else in `Config.h` (device identity, credentials, constants) — the two stay
separate so rewiring a board never touches the rest of the configuration.

All pins are drawn from the boot/Wi-Fi-safe pool for the 38-pin ESP32
variant — see [`docs/schematic/pin-map.md`](../../docs/schematic/pin-map.md)
for the avoidance table and wiring notes.

Each board only includes the modules it needs:

| Module               | Monitor (gateway) | Chiller | Dehumidifier |
| -------------------- | :---------------: | :------: | :----------: |
| `TemperatureSensor`  | Yes (4x)          | Yes (2x) | —            |
| `HumiditySensor`     | —                 | —        | Yes          |
| `SsrOutput`          | —                 | Yes (×2) | Yes          |
| `WifiProvisioner`    | Yes               | —        | —            |
| `EspNowTransport`    | Yes               | Yes      | Yes          |
| `JsonProtocol`       | Yes               | Yes      | Yes          |
| `FirebaseSync`       | Yes               | —        | —            |
| `WeatherApi`         | Yes               | —        | —            |
| `ClimateControl`     | Yes               | —        | —            |

## Why modules are duplicated across folders

Arduino IDE compiles each sketch folder standalone. Common modules
(`EspNowTransport`, `JsonProtocol`, `SsrOutput`, `TemperatureSensor`) are
therefore **copied into each sketch that needs them**. Keep them in sync, or
extract them into a shared custom library later (e.g. `libraries/RadiantCore/`)
if the duplication becomes a burden.

## Required libraries (Arduino IDE: Tools > Manage Libraries)

| Library             | Used by         | Notes                                              |
| ------------------- | --------------- | -------------------------------------------------- |
| `DallasTemperature` | monitor, chiller| depends on OneWire                                |
| `OneWire`           | monitor, chiller|                                                    |
| `DHT sensor library`| dehumidifier    | DHT22                                             |
| `ArduinoJson`       | all             | v7 (`JsonDocument`)                               |
| `FirebaseClient`    | monitor         | Mobizt; **v2.2.x+** (modern API: SSL client + SSE stream; the old `Firebase-ESP-Client` is deprecated) |
| `WiFiManager`       | monitor         | tzapu - captive-portal WiFi provisioning            |

**ESP32 core 3.x** (by Espressif) is required — `EspNowTransport` and
`FirebaseSync` use the core-3.x / FirebaseClient-2.2.x APIs.

ESP-NOW (`esp_now.h`), WiFi, and `HTTPClient` (used by `WeatherApi`) are
built into the Arduino ESP32 core. The gateway fetches outdoor weather
from WeatherAPI.com itself; the API key is **managed by the Flutter app**
and delivered at runtime via `radiant/config/weather_key` — it is never
compiled into the firmware (see `docs/api.md §4`). Failed or stale fetches
(`WEATHER_STALE_S`, default 1 h) disable weather demand and fall back to
the indoor dew point.

WiFi credentials are **not** stored in code — they are entered once through
the WiFiManager captive portal (AP `RadiantCooling-AP`). Hold the gateway's
reset button (GPIO 25, see `PINS_CONFIG.h`) for ~3 s to erase them and
re-provision.

Firebase is **two-way** on the gateway (Mobizt `FirebaseClient`, async):
`setJson()`/`updateJson()` write telemetry/state to `radiant/telemetry/*`,
and `stream()` listens to `radiant/config` in real time. Enable
**Email/Password** auth in the Firebase console (create a dedicated gateway
account, e.g. `gateway@radiant-cooling.local`) and put the Web API key +
credentials in `FirebaseConfig.cpp` (`getApiKey()`, `getAuthEmail()`,
`getAuthPassword()`; copy from `FirebaseConfig.cpp.example` — it is
git-ignored). Note: `docs/firebase-security-rules.json` identifies the
gateway by `auth.token.email`, so the gateway must sign in with
email/password (not anonymous) when those rules are used.

## Implemented behavior

- **ESP-NOW receive** on every board is decoupled through a FreeRTOS queue:
  the callback only enqueues raw bytes; `loop()` drains, decodes
  (`JsonProtocol`), and applies/handles the message.
- **ESP-NOW channel and power save:** every board calls `WiFi.setSleep(false)`
  - a sleeping receiver misses the MAC-layer ACK window, so the data still
  arrives while every send logs `tx FAIL`. Peers never join an AP, so they pin
  their radio with `ESP_NOW_CHANNEL` in `Config.h`; the gateway always follows
  the router. All boards must share one channel: the gateway prints
  `CHANNEL MISMATCH` at boot when they do not, and both ends print their
  channel (`wifi` on the gateway, `status` on the peers).
- **Gateway tasking**: Firebase and the WeatherAPI HTTPS fetch run on a
  dedicated 20 KB `cloudTask`, never on `loop()`. The Arduino `loopTask`
  stack is only 8 KB and a TLS handshake overflowed it (`Stack canary
  watchpoint triggered (loopTask)`), so `loop()` now only enqueues
  `path + json` pairs on a FreeRTOS queue: the cloud task drains them
  (latest write per path wins, so a burst of ESP-NOW telemetry costs one
  connection open per path) and reloads its own weather inputs. Keep new
  `Firebase.*` / `HTTPClient` calls on the cloud task - in `loop()` they
  will panic again.
- **Weather**: the gateway picks the WeatherAPI key up from Firebase (config
  stream, plus a REST re-read while it has none, hourly afterwards, and
  straight after a failed fetch), fetches `current.json` every
  `WEATHER_POLL_S`, and publishes the reading to
  `radiant/telemetry/weather/latest` as well as into its own telemetry.
- **Gateway** reads the 4 ceiling pipe temperatures (4x DS18B20, one 1-Wire
  bus each: sensors 1-2 feed the supply/return telemetry), caches
  the latest peer readings for the chiller computation, forwards peer
  `telemetry`/`state` to Firebase, publishes its own telemetry + a retained
  heartbeat (`radiant/heartbeat/monitor`, includes the Wi-Fi channel), sends
  `set_pumps` on decision changes, and applies or forwards `radiant/config`
  stream changes. Cooling demand comes from the dehumidifier's DHT22 indoor
  temperature; the anti-condensation floor protects the coldest pipe/tank.
- **Chiller** executes `set_pumps` (`on`/`off`), reports a `state` message
  on pump changes, and streams `water_temp_c` telemetry every
  `TELEMETRY_S`.
- **Dehumidifier** applies `config` (setpoint/deadband) and `cmd`
  (`set_humidity_target`, `enable`/`disable`, `reset`), runs the 55 %%RH
  hysteresis loop, and streams `temp_c`/`humidity_pct` telemetry. After 3
  consecutive sensor read failures it fails safe (dehumidifier off).

## Serial test console (manual component testing)

`RadiantCoolingMonitor.ino`, `WaterChillerController.ino` and
`DehumidifierController.ino` have a small
test console built on `Serial.readStringUntil('\n')` + an `if / else if`
command chain, polled at the top of `loop()`. Open the Serial Monitor at
**115200 baud** with the **Newline** line ending, then type `help`:

| Command | Board | What it exercises |
| ------- | ----- | ----------------- |
| `temps`, `temp <0-3>` | monitor | the 4 ceiling DS18B20 sensors, one bus each (GPIO 23 / 21 / 18 / 5; DISCONNECTED / 85 °C / out-of-range diagnostics) |
| `button`, `button test` | monitor | WiFi reset button (GPIO 25): one-shot read, or a 10 s press/release edge watch |
| `wifi` | monitor | WiFi link (SSID / IP / RSSI / MAC) |
| `wx` | monitor | WeatherAPI fetch on demand (updates the cached outdoor inputs) |
| `mode manual\|auto` | monitor | suspend/resume auto pump control |
| `pumps on\|off` | monitor | ESP-NOW TX to the chiller (`set_pumps`) |
| `espnow chiller\|dh` | monitor | ESP-NOW ping (`cmd: ping`) - peers log `unknown cmd: ping` on arrival; TX result prints as `[espnow] tx OK -> ..` |
| `status`, `reboot` | monitor | dump current inputs/control state, restart |
| `temps`, `temp outgoing\|ingoing` | chiller | both tank DS18B20 sensors (out=GPIO 5, in=GPIO 18) with DISCONNECTED / 85 °C / out-of-range diagnostics |
| `ssr` | chiller | compressor + pump relay output states (GPIO 23 / 19) |
| `compressor on\|off\|toggle` | chiller | compressor SSR (GPIO 23) by hand |
| `pump1 on\|off\|toggle` | chiller | the pump relay (GPIO 19) by hand |
| `outputs on\|off` | chiller | compressor + pump together via `setPumps()` - also sends a `state` frame to the gateway |
| `espnow` | chiller | status frame to the gateway (link test) |
| `status`, `reboot` | chiller | fresh sensor read + output states, restart |
| `dht`, `dht test` | dehumidifier | DHT22 single read, or 5 samples 2 s apart (stability) |
| `ssr`, `ssr on\|off\|toggle` | dehumidifier | SSR output (GPIO 23) state and manual drive |
| `setpoint <0-100>`, `deadband <0-30>` | dehumidifier | humidity hysteresis thresholds by hand |
| `enable`, `disable` | dehumidifier | `disable` forces the SSR off so `ssr on/off` tests are not overridden by the control loop |
| `espnow` | dehumidifier | status frame to the gateway (link test) |
| `status`, `reboot` | dehumidifier | dump current state, restart |

There is no button on the chiller or dehumidifier boards (`PINS_CONFIG.h`
only defines the sensor/output pins) - the monitor's WiFi reset button is
the only one. Type an unknown command to get a reminder of the
list; the console never mutates Firebase, only local state and the
peer-facing commands listed above.

## Unit tests

Host-machine tests for the pure-math modules live in
[`src/esp/tests/`](tests/README.md) (stub `Arduino.h`, no Arduino toolchain
needed). Currently covered: `ClimateControl` — Magnus dew point + the pump
decision logic (weather/sensor demands, condensation protection,
hysteresis, reference dew point selection).

```bash
cd src/esp/tests
./run_tests.sh     # requires g++ (Git Bash / WSL on Windows works)
```

## Building (headless, arduino-cli)

A local toolchain is set up under `.arduino/` (git-ignored) — its
`arduino-cli.yaml` keeps the core, libraries and downloads inside the repo, so
no global install is needed:

```bash
./.arduino/bin/arduino-cli.exe compile --fqbn \
  esp32:esp32:esp32:PartitionScheme=huge_app --build-path .arduino/build/monitor \
  src/esp/RadiantCoolingMonitor
```

The **`PartitionScheme=huge_app`** option is required: the gateway sketch
(Firebase + TLS + WiFiManager + ArduinoJson) links to ~1.41 MB, which is over
the 1.31 MB app partition of the default scheme. Select the same option in the
Arduino IDE (Tools → Partition Scheme → "Huge APP (3MB No OTA/1MB SPIFFS)").

## Working with the sketches

1. Add the ESP32 board manager URL and install "esp32 by Espressif".
2. Install the libraries above.
3. Open each board folder as a sketch in Arduino IDE, set the right board
   and COM port, and upload.
4. Fill in the config headers before flashing: `Config.h` (peer/gateway
   MACs, `SYSTEM_ID`), `FirebaseConfig.cpp` (Firebase URL + Web API key -
   copy from `FirebaseConfig.cpp.example`; it is git-ignored), and check
   `PINS_CONFIG.h` (pins). WiFi credentials are entered via the captive
   portal on first boot - nothing to fill in. The WeatherAPI key is
   entered in the Flutter app (delivered to the gateway at runtime); the
   weather location `WEATHER_LOCATION` is set in `Config.h`.

## References

- Architecture: [`docs/diagrams/system-architecture.md`](../../docs/diagrams/system-architecture.md)
- Hardware: [`docs/diagrams/block-diagram.md`](../../docs/diagrams/block-diagram.md)
- Control logic: [`docs/diagrams/flow-chart.md`](../../docs/diagrams/flow-chart.md)
- ESP-NOW + Firebase protocol: [`docs/api.md`](../../docs/api.md)
- Pin map (38-pin board, boot/Wi-Fi-safe): [`docs/schematic/pin-map.md`](../../docs/schematic/pin-map.md)
- References (ESP-NOW, Firebase, sensors, WiFiManager…): [`references/`](../../references/)
