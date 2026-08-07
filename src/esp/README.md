# ESP32 Firmware

Three standalone Arduino sketches. **Every component type and every external
library is encapsulated in its own class module** — the `.ino` files are glue
only (wiring + control logic).

## Board folders

| Folder                     | Hardware                                                     | Role     |
| -------------------------- | ------------------------------------------------------------ | -------- |
| `RadiantCoolingMonitor/`   | 6x DS18B20 (1-Wire)                                          | Gateway  |
| `WaterChillerController/`  | 1x DS18B20, 2x SSR -> 2 water pumps                          | Peer     |
| `DehumidifierController/`  | 1x DHT22, 1x SSR -> dehumidifier                             | Peer     |

## Module layout (same pattern in every sketch folder)

```
<BoardName>/
├── <BoardName>.ino     # glue: instantiate components, setup()/loop(), control logic
├── Config.h            # board config (MACs, Firebase keys, constants); includes PINS_CONFIG.h + WEATHER_CONFIG.h
├── PINS_CONFIG.h       # pin assignments - all GPIO wiring for the board
├── WEATHER_CONFIG.h    # WeatherAPI credentials (GIT-IGNORED; copy from WEATHER_CONFIG.example.h)
├── TemperatureSensor.{h,cpp}   # component: DS18B20   (wraps OneWire + DallasTemperature)
├── HumiditySensor.{h,cpp}      # component: DHT22     (wraps DHT)
├── SsrOutput.{h,cpp}           # component: SSR output (wraps Arduino digital I/O)
├── WifiProvisioner.{h,cpp}     # wifi:     WiFiManager captive portal (wraps WiFiManager) [gateway only]
├── EspNowTransport.{h,cpp}     # comms:    ESP-NOW    (wraps WiFi + esp_now)
├── JsonProtocol.{h,cpp}        # protocol: JSON envelope (wraps ArduinoJson)
├── FirebaseSync.{h,cpp}        # cloud:    Firebase RTDB set/update + realtime stream (wraps FirebaseClient) [gateway only]
├── WeatherApi.{h,cpp}          # cloud:    WeatherAPI.com current weather (wraps HTTPClient) [gateway only]
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
| -------------------- | :---------------: | :-----: | :----------: |
| `TemperatureSensor`  | ✅ (6x)           | ✅ (1x) | —            |
| `HumiditySensor`     | —                 | —       | ✅           |
| `SsrOutput`          | —                 | ✅ (×2) | ✅           |
| `WifiProvisioner`    | ✅                | —       | —            |
| `EspNowTransport`    | ✅                | ✅      | ✅           |
| `JsonProtocol`       | ✅                | ✅      | ✅           |
| `FirebaseSync`       | ✅                | —       | —            |
| `WeatherApi`         | ✅                | —       | —            |
| `ClimateControl`     | ✅                | —       | —            |

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
| `FirebaseClient`    | monitor         | Mobizt; the old `Firebase-ESP-Client` is deprecated |
| `WiFiManager`       | monitor         | tzapu - captive-portal WiFi provisioning            |

ESP-NOW (`esp_now.h`), WiFi, and `HTTPClient` (used by `WeatherApi`) are
built into the Arduino ESP32 core. A **WeatherAPI.com API key** is required
in the gateway's `Config.h` (`WEATHER_API_KEY`).

WiFi credentials are **not** stored in code — they are entered once through
the WiFiManager captive portal (AP `RadiantCooling-AP`). Hold the gateway's
reset button (GPIO 33, see `PINS_CONFIG.h`) for ~3 s to erase them and
re-provision.

Firebase is **two-way** on the gateway (Mobizt `FirebaseClient`, async):
`setJson()`/`updateJson()` write telemetry/state to `radiant/telemetry/*`,
and `stream()` listens to `radiant/config` in real time. Enable **Anonymous**
or **Email/Password** auth in the Firebase console and put the Web API key
in `FIREBASE_API_KEY` (see `Config.h`).

## Working with the sketches

1. Add the ESP32 board manager URL and install "esp32 by Espressif".
2. Install the libraries above.
3. Open each board folder as a sketch in Arduino IDE, set the right board
   and COM port, and upload.
4. Fill in the config headers before flashing: `Config.h` (Firebase keys +
   peer/gateway MACs), `WEATHER_CONFIG.h` (WeatherAPI key - copy from
   `WEATHER_CONFIG.example.h`; it is git-ignored), and check `PINS_CONFIG.h`
   (pins). WiFi credentials are entered via the captive portal on first
   boot - nothing to fill in.

## References

- Architecture: [`docs/diagrams/system-architecture.md`](../../docs/diagrams/system-architecture.md)
- Hardware: [`docs/diagrams/block-diagram.md`](../../docs/diagrams/block-diagram.md)
- Control logic: [`docs/diagrams/flow-chart.md`](../../docs/diagrams/flow-chart.md)
- ESP-NOW + Firebase protocol: [`docs/api.md`](../../docs/api.md)
- Pin map (38-pin board, boot/Wi-Fi-safe): [`docs/schematic/pin-map.md`](../../docs/schematic/pin-map.md)
- References (ESP-NOW, Firebase, sensors, WiFiManager…): [`references/`](../../references/)
