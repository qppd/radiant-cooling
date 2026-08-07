# ESP32 38-Pin Variant — Pinout, Boot & Wi-Fi Conflicts

> Why the pins in every `PINS_CONFIG.h` were chosen for the 38-pin boards
> (WROOM-32 DevKit / NodeMCU-32S) with Wi-Fi + ESP-NOW running.

## Pins to AVOID

| Pins        | Reason                                                                 |
| ----------- | ---------------------------------------------------------------------- |
| `0`, `2`, `5`, `12`, `15` | **Strapping pins** — sampled at reset. GPIO 12 high at boot forces 1.8 V flash → boot failure. GPIO 2 often drives the onboard LED. |
| `6`–`11`    | Wired to the **internal SPI flash** — never usable.                     |
| `1`, `3`    | **UART0** — USB programming + Serial Monitor.                          |
| `0, 2, 4, 12, 13, 14, 15, 25, 26, 27` | **ADC2** — `analogRead()` fails while Wi-Fi is active (and Wi-Fi is always on here). |
| `34`–`39`   | Input-only (no outputs, no internal pull-ups).                          |

## Digital-safe pool (used by this project)

`18, 19, 21, 22, 23, 32, 33`

| Board                   | Pin | Use                    |
| ----------------------- | --- | ---------------------- |
| `RadiantCoolingMonitor` | 18  | 1-Wire (6× DS18B20)    |
| `RadiantCoolingMonitor` | 33  | WiFi reset button      |
| `WaterChillerController`| 19, 21 | SSR pump 1, pump 2  |
| `WaterChillerController`| 22  | 1-Wire (1× DS18B20)    |
| `DehumidifierController`| 23  | SSR dehumidifier       |
| `DehumidifierController`| 32  | DHT22                  |

## Key boot notes

- **GPIO 12** must be LOW at boot (pull-up there can brick flashing).
- **GPIO 0** LOW at boot = download/flash mode (handled by the USB-UART).
- ADC2 pins are safe for **digital** I/O, but were avoided entirely here to
  keep the option of analog reads and zero Wi-Fi surprises.

## Links

- ESP32 datasheet: <https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf>
- RNT — ESP32 pinout: <https://randomnerdtutorials.com/esp32-pinout-reference-gpios/>
- Stack Overflow — ADC2 + Wi-Fi conflict: <https://stackoverflow.com/questions/48100961/esp32-adc-accuracy-for-voltage-measurement>
