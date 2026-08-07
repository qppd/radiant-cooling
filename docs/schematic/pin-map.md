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
| `WaterChillerController`| 19  | SSR → water pump 1                 | SSR input (see wiring note)  |
| `WaterChillerController`| 21  | SSR → water pump 2                 | SSR input (see wiring note)  |
| `WaterChillerController`| 22  | 1-Wire bus (1x DS18B20)            | 4.7kΩ pull-up to 3V3         |
| `DehumidifierController`| 23  | SSR → dehumidifier                 | SSR input (see wiring note)  |
| `DehumidifierController`| 32  | DHT22 data                         | 10kΩ pull-up to 3V3          |
| (spare)                 | 33  | —                                  | available on all boards      |

## Wiring notes

- **1-Wire (DS18B20):** a single data pin handles all sensors in parallel —
  add a **4.7 kΩ pull-up** from the data line to 3V3. Use parasite power only
  if necessary (prefer external 3V3 supply).
- **DHT22:** data pin needs a **10 kΩ pull-up** to 3V3.
- **SSR modules:** most SSR modules have an input that can be driven directly
  from a 3.3V GPIO (3-32V DC input). Confirm the module's input spec; if it
  needs more drive, buffer with a transistor/MOSFET.
- **ESP-NOW needs no pins** — it uses the radio. All boards run in STA mode
  (the gateway on the router channel; peers match that channel).

## Power

Each board is fed from its own 3.3V regulator (onboard). Do **not** power
sensors or SSR inputs from the 5V pin. Total GPIO sink/source is shared — use
an external supply for the pump/dehumidifier loads (the SSR isolates them).
