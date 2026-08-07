# ESP32 Pin Map (38-pin variant)

> Pin assignments for the three ESP32 boards. All boards run Wi-Fi/ESP-NOW,
> so every sensor/component pin is selected from the **digital-safe pool**
> that does not conflict with boot mode, the SPI flash, UART0, or ADC2.
>
> The authoritative pin values live in each sketch's `PINS_CONFIG.h`; this
> document is the reference for why those pins were chosen.

## Pins to AVOID on the 38-pin board

| Pins        | Why                                                             |
| ----------- | --------------------------------------------------------------- |
| `0`, `2`, `5`, `12`, `15` | **Strapping pins** — sampled at reset to select boot mode. GPIO12 high at boot forces 1.8V flash → boot failure. GPIO2 often drives the onboard LED. |
| `6`–`11`    | **SPI flash** — wired to the internal flash chip. Never use; breaks boot. |
| `1`, `3`    | **UART0** — USB programming + Serial Monitor.                   |
| `0, 2, 4, 12, 13, 14, 15, 25, 26, 27` | **ADC2** — `analogRead()` fails while Wi-Fi is active (and we always run Wi-Fi/ESP-NOW). Avoid entirely. |
| `34`–`39`   | **Input-only** — no output capability. Fine for inputs only.     |

## Digital-safe pool (used by this project)

`18`, `19`, `21`, `22`, `23`, `32`, `33`

These are neither strapping, flash, UART0, nor ADC2 pins — safe for digital
I/O with Wi-Fi and ESP-NOW running.

## Per-board assignments

| Board                   | Pin | Component                          | Notes                        |
| ----------------------- | --- | ---------------------------------- | ---------------------------- |
| `RadiantCoolingMonitor` | 18  | 1-Wire bus (6x DS18B20)            | 4.7kΩ pull-up to 3V3         |
| `RadiantCoolingMonitor` | 33  | WiFi reset button                   | momentary switch to GND, INPUT_PULLUP; hold 3 s to erase WiFi credentials |
| `WaterChillerController`| 19  | SSR → water pump 1                 | SSR input (see wiring note)  |
| `WaterChillerController`| 21  | SSR → water pump 2                 | SSR input (see wiring note)  |
| `WaterChillerController`| 22  | 1-Wire bus (1x DS18B20)            | 4.7kΩ pull-up to 3V3         |
| `DehumidifierController`| 23  | SSR → dehumidifier                 | SSR input (see wiring note)  |
| `DehumidifierController`| 32  | DHT22 data                         | 10kΩ pull-up to 3V3          |
| (spare)                 | 19, 21, 22, 23, 32    | —                         | free on the gateway board   |

## Wiring notes

- **1-Wire (DS18B20):** a single data pin handles all sensors in parallel —
  add a **4.7 kΩ pull-up** from the data line to 3V3. Use parasite power only
  if necessary (prefer external 3V3 supply).
- **DHT22:** data pin needs a **10 kΩ pull-up** to 3V3.
- **SSR modules:** most SSR modules have an input that can be driven directly
  from a 3.3V GPIO (3-32V DC input). Confirm the module's input spec; if it
  needs more drive, buffer with a transistor/MOSFET.
- **WiFi reset button:** a momentary push button wired from GPIO 33 to GND
  (the internal pull-up is enabled in code). Hold for ~3 s to erase the
  saved WiFi credentials (WiFiManager) and restart — a captive portal then
  opens for new SSID/password. Holding it at power-up also erases the
  credentials (the portal opens without a restart).
- **ESP-NOW needs no pins** — it uses the radio. All boards run in STA mode.
- **Channel lock with WiFiManager:** the gateway connects to the router's
  channel automatically, so its ESP-NOW channel is whatever the router uses.
  The peers (STA mode, not associated) default to channel 1 and will not
  hear the gateway unless they match — **fix the router to channel 1, 6, or
  11** and/or configure the peers' channel to match the gateway.

## Power

Each board is fed from its own 3.3V regulator (onboard). Do **not** power
sensors or SSR inputs from the 5V pin. Total GPIO sink/source is shared — use
an external supply for the pump/dehumidifier loads (the SSR isolates them).

## References

- ESP32 38-pin pinout, boot & Wi-Fi conflicts: [`references/esp32-38pin-pinout.md`](../../references/esp32-38pin-pinout.md)
- SSR wiring: [`references/solid-state-relay.md`](../../references/solid-state-relay.md)
- DS18B20 / DHT22 pull-up notes: [`references/ds18b20-temperature-sensor.md`](../../references/ds18b20-temperature-sensor.md), [`references/dht22-humidity-sensor.md`](../../references/dht22-humidity-sensor.md)
