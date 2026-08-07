# Arduino IDE + ESP32 Setup

> Toolchain used for all three firmware sketches.

## Steps

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software).
2. **Add the ESP32 board manager URL** in *File → Preferences*:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. *Tools → Board → Boards Manager* → search **esp32** → install
   **"esp32 by Espressif"**.
4. Install libraries via *Tools → Manage Libraries*:

| Library                | Boards                 |
| ---------------------- | ---------------------- |
| `OneWire`              | monitor, chiller       |
| `DallasTemperature`    | monitor, chiller       |
| `DHT sensor library`   | dehumidifier           |
| `ArduinoJson`          | all                    |
| `FirebaseClient`       | monitor (Mobizt)       |
| `WiFiManager`          | monitor (tzapu)        |

5. Open each board folder in `src/esp/` as a sketch, select the right ESP32
   board + COM port, and upload.

## Notes

- ESP-NOW (`esp_now.h`), `WiFi`, and `HTTPClient` are built into the core —
  no extra installs.
- `.h` / `.cpp` files inside a sketch folder are compiled automatically.

## Links

- Arduino-ESP32 core: <https://github.com/espressif/arduino-esp32>
- Board manager JSON: <https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json>
- Installing ESP32 boards: <https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html>
