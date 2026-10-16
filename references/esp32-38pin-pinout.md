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

`5, 18, 19, 21, 22, 23, 25, 32, 33`

| Board                   | Pin | Use                    |
| ----------------------- | --- | ---------------------- |
| `RadiantCoolingMonitor` | 23, 21, 18, 19 | 1-Wire (4× ceiling DS18B20, one bus each) |
| `RadiantCoolingMonitor` | 25  | WiFi reset button      |
| `WaterChillerController`| 23, 19 | SSR compressor, SSR pump |
| `WaterChillerController`| 18, 5  | 1-Wire (ingoing, outgoing DS18B20) |
| `DehumidifierController`| 23  | SSR dehumidifier       |
| `DehumidifierController`| 25  | DHT22                  |

## Key boot notes

- **GPIO 12** must be LOW at boot (pull-up there can brick flashing).
- **GPIO 0** LOW at boot = download/flash mode (handled by the USB-UART).
- **GPIO 5** is a strapping pin, but only selects SDIO timing (unused — we
  boot from flash). The chiller's outgoing DS18B20 sits on it; the sensor's
  4.7 kΩ pull-up holds it HIGH at boot, i.e. its default state.
- **GPIO 25** is an ADC2 pin — `analogRead()` won't work there with Wi-Fi
  on, but digital input with the internal pull-up (the gateway's reset
  button) is fine.
- ADC2 pins are safe for **digital** I/O, but were avoided for analog reads
  to keep the option of analog sensing and zero Wi-Fi surprises.

## Links

- ESP32 datasheet: <https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf>
- RNT — ESP32 pinout: <https://randomnerdtutorials.com/esp32-pinout-reference-gpios/>
- Stack Overflow — ADC2 + Wi-Fi conflict: <https://stackoverflow.com/questions/48100961/esp32-adc-accuracy-for-voltage-measurement>
