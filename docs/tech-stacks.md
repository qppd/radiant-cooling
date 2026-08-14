# Technology Stacks

> Languages, frameworks, and tools used across the Radiant Cooling System.

## Firmware (ESP32 controllers)

| Layer      | Choice                                            | Notes                                              |
| ---------- | ------------------------------------------------- | -------------------------------------------------- |
| MCU        | ESP32                                             | Wi-Fi + BLE in one module, ample GPIO              |
| Language   | C / C++                                           | Standard for embedded controllers                  |
| Build      | Arduino IDE                                       | One sketch per controller; ESP32 core via Board Manager |
| Framework  | Arduino (`esp32` core)                            | Decided: Arduino IDE + Arduino framework           |
| Mesh       | **ESP-NOW** (built into the ESP32 core)           | Board-to-board comms, no router/broker required    |
| WiFi       | **WiFiManager** (tzapu) — gateway only            | Branded captive portal (static AP IP), auto-reconnect; no hardcoded SSID/password |
| Cloud      | **FirebaseClient** (Mobizt) — gateway only        | Async Firebase Realtime Database REST client; the older Firebase-ESP-Client is deprecated |
| Encoding   | ArduinoJson                                       | Compact JSON payloads (≤ 250 B) over ESP-NOW    |
| Sensors     | OneWire / DallasTemperature, DHT                  | Install via Arduino Library Manager              |
| Code layout | One class module per component + per library      | See [`src/esp/README.md`](../src/esp/README.md)  |

## Companion application (Flutter, Android)

| Layer      | Choice                       | Notes                                         |
| ---------- | ---------------------------- | --------------------------------------------- |
| UI         | Flutter                      | Targeting **Android** (decided)               |
| Language   | Dart                         |                                               |
| State mgmt | Plain `setState` + `StreamBuilder` | No state-management package (kept simple for a 3-screen app) |
| Firebase   | `firebase_core`, `firebase_auth`, `firebase_database`, `shared_preferences` | Email/password login, Realtime Database streams, local link/key storage |

## Communication & Cloud

| Layer           | Choice                              | Notes                                 |
| --------------- | ----------------------------------- | ------------------------------------- |
| Board-to-board  | ESP-NOW                             | 250 B max payload; protocol in [`api.md`](api.md) |
| Board-to-cloud  | HTTPS REST to Firebase RTDB         | Gateway board only                    |
| Cloud database  | Firebase Realtime Database          | JSON tree; path scheme in `api.md`    |
| Weather data    | WeatherAPI.com (free tier)          | Gateway fetches it; the app delivers the API key via `config/weather_key` |
| Provisioning    | WiFiManager captive portal (gateway)| Branded portal (AP `RadiantCooling-AP`, 192.168.4.1); enter SSID/password once; reset button (GPIO 33) to re-provision |

## Documentation

| Tool        | Purpose                        |
| ----------- | ------------------------------ |
| Markdown    | All documentation              |
| Mermaid     | Diagrams (architecture, flows) |

## Tooling & DevOps

| Tool         | Purpose                              |
| ------------ | ------------------------------------ |
| Git / GitHub | Version control & hosting            |
| Arduino IDE  | Firmware build, upload, serial monitor|
| Flutter SDK  | Android app build & test             |
| Firebase CLI | Project config, emulator (optional)  |
| CI (future)  | Optional GitHub Actions for firmware + app builds |

## Alternatives considered

- **FreeRTOS directly on ESP32** instead of Arduino — more control,
  more effort; revisit only if scheduling needs grow.
- **PlatformIO** instead of Arduino IDE — nicer CLI/CI story, but Arduino IDE
  was chosen for simplicity of development and flashing.
- **MQTT broker (Mosquitto)** instead of ESP-NOW — great for larger fleets,
  but requires a router/broker on site; ESP-NOW fits 3 boards with no extra
  infrastructure.
- **Cloud Firestore** instead of Realtime Database — richer app queries, but
  much harder for ESP32 to authenticate against; RTDB is the ESP32-friendly
  choice.

## References

Per-topic reference material lives in [`references/`](../references/) —
ESP-NOW, FirebaseClient, WiFiManager, sensors, SSR, WeatherAPI, ESP32
pinout, dew point, and toolchain guides.
