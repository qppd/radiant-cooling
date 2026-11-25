# ESP32 Pin Map (38-pin variant)

> Interactive 3D diagram: [`wiring-diagram.html`](wiring-diagram.html)
> (Three.js, top view — hover a component to see its wiring). The pins
> below are the authoritative values.

> Pin assignments for the three ESP32 boards. All boards run Wi-Fi/ESP-NOW,
> so every sensor/component pin is selected from the **digital-safe pool**
> that does not conflict with boot mode, the SPI flash, UART0, or ADC2.
>
> The authoritative pin values live in each sketch's `PINS_CONFIG.h`; this
> document is the reference for why those pins were chosen.

## Pins to AVOID on the 38-pin board

| Pins        | Why                                                             |
| ----------- | --------------------------------------------------------------- |
| `0`, `2`, `5`, `12`, `15` | **Strapping pins** — sampled at reset to select boot mode. GPIO12 high at boot forces 1.8V flash → boot failure. GPIO2 often drives the onboard LED. **GPIO5** only selects SDIO timing (not used — we boot from flash); the chiller's outgoing DS18B20 sits on it with a 4.7 kΩ pull-up to 3V3, which holds it at its default HIGH at boot, so it is safe there. |
| `6`–`11`    | **SPI flash** — wired to the internal flash chip. Never use; breaks boot. |
| `1`, `3`    | **UART0** — USB programming + Serial Monitor.                   |
| `0, 2, 4, 12, 13, 14, 15, 25, 26, 27` | **ADC2** — `analogRead()` fails while Wi-Fi is active (and we always run Wi-Fi/ESP-NOW). Avoid for analog reads; plain digital I/O (e.g. the gateway's reset button on GPIO25) is fine. |
| `34`–`39`   | **Input-only** — no output capability. Fine for inputs only.     |

## Digital-safe pool (used by this project)

`5`, `18`, `19`, `21`, `22`, `23`, `25`, `32`, `33`

These are neither flash nor UART0 pins — safe for digital I/O with Wi-Fi and
ESP-NOW running (GPIO5 is strapping but held at its default by the sensor
pull-up; GPIO25 is ADC2 but only used as a digital input).

## Per-board assignments

| Board                   | Pin | Component                          | Notes                        |
| ----------------------- | --- | ---------------------------------- | ---------------------------- |
| `RadiantCoolingMonitor` | 23  | DS18B20 ceiling sensor 1           | own 1-Wire bus; 4.7kΩ pull-up to 3V3 |
| `RadiantCoolingMonitor` | 21  | DS18B20 ceiling sensor 2           | own 1-Wire bus; 4.7kΩ pull-up to 3V3 |
| `RadiantCoolingMonitor` | 18  | DS18B20 ceiling sensor 3           | own 1-Wire bus; 4.7kΩ pull-up to 3V3 |
| `RadiantCoolingMonitor` | 5   | DS18B20 ceiling sensor 4           | own 1-Wire bus; 4.7kΩ pull-up to 3V3; strapping pin held at its default HIGH by the pull-up, so boot is unaffected |
| `RadiantCoolingMonitor` | 25  | WiFi reset button                   | momentary switch to GND, INPUT_PULLUP; hold 3 s to erase WiFi credentials |
| `WaterChillerController`| 23  | SSR1 → compressor + condenser fan + chiller pump | SSR input (see wiring note) |
| `WaterChillerController`| 19  | Relay IN → outgoing pump            | Relay input (see wiring note)|
| `WaterChillerController`| 18  | DS18B20 → ingoing water (to tank)   | DAT; own 1-Wire bus, pull-up |
| `WaterChillerController`| 5   | DS18B20 → outgoing water (from tank)| DAT; own bus; strapping pin held HIGH by the pull-up |
| `DehumidifierController`| 23  | SSR → dehumidifier                 | SSR input (see wiring note)  |
| `DehumidifierController`| 25  | DHT22 data                         | 10kΩ pull-up to 3V3          |
| (spare)                 | 21, 22, 32, 33    | —                         | free on the gateway + chiller boards |

## Chiller output grouping

The chiller has two SSR/relay outputs, both controlled independently in
manual mode and together in auto mode:

| Output | GPIO | Load |
| ------ | ---- | ---- |
| SSR1   | 23   | Compressor + condenser fan + chiller pump (parallel on one SSR) |
| SSR2   | 19   | Outgoing pump |

The compressor and condenser fan must always run together, so they share
SSR1. The chiller pump (water circulator inside the chiller unit) is also
wired to SSR1 because it must run whenever the compressor runs — all three
loads are energized in parallel from a single SSR output. This eliminates the
need for a third output and guarantees the condenser fan and water pump are
never out of sync with the compressor.

The app labels SSR1 as **Compressor + condenser fan** and SSR2 as
**Outgoing pump**.

## Wiring notes

- **DS18B20 adapter modules:** connect each module's `DAT` to its assigned
  GPIO, `GND` to ESP32 `GND`, and `VCC` to `3V3`. These sensors use separate
  1-Wire buses in this controller. Check the module: many adapters already
  include the required **4.7 kΩ pull-up** from `DAT` to `VCC`; add one only
  if it is absent. Do not connect adapter `VCC` to an ESP32 GPIO.
- **DHT22:** data pin needs a **10 kΩ pull-up** to 3V3.
- **Dehumidifier board power:** connect the regulated **5 V, 3 A** supply to
  ESP32 `VIN` and `GND`. Connect DHT22 `VCC` to `3V3`, DHT22 `GND` to ESP32
  `GND`, and the SSR input ground to the same common low-voltage `GND`.
- **SSR modules:** most SSR modules have an input that can be driven directly
  from a 3.3V GPIO (3-32V DC input). Confirm the module's input spec; if it
  needs more drive, buffer with a transistor/MOSFET.
- **WiFi reset button:** a momentary push button wired from GPIO 25 to GND
  (the internal pull-up is enabled in code). Hold for ~3 s to command the
  pumps **off**, erase the saved WiFi credentials (WiFiManager) and restart
  — a captive portal then opens for new SSID/password. Holding it at
  power-up also erases the credentials (the portal opens without a restart).
- **ESP-NOW needs no pins** — it uses the radio. All boards run in STA mode.
- **Channel lock with WiFiManager:** while associated the gateway follows the
  router's channel; when the AP is missing it re-pins its radio to
  `ESP_NOW_CHANNEL` (default 6, `Config.h`), matching the peers that pin the
  same channel. **Fix the router to that channel** or every ESP-NOW send
  fails (the gateway prints a `CHANNEL MISMATCH` warning and control fails
  safe with the pumps off).

## Power

Each board is fed by its **own** AC-DC power supply: **220 V AC → 5 V DC,
3 A**, connected to the board's `5V`/`VIN` pin (the onboard regulator steps
it down to 3.3 V). There is **no shared rail between boards** — each board
is galvanically isolated. Do **not** power sensors or SSR inputs from the 5V
pin. Total GPIO sink/source is shared — use the SSR to switch the
pump/dehumidifier mains loads (the SSR isolates them from the ESP32).

## References

- ESP32 38-pin pinout, boot & Wi-Fi conflicts: [`references/esp32-38pin-pinout.md`](../../references/esp32-38pin-pinout.md)
- Power supplies: [`references/power-supply.md`](../../references/power-supply.md)
- SSR wiring: [`references/solid-state-relay.md`](../../references/solid-state-relay.md)
- DS18B20 / DHT22 pull-up notes: [`references/ds18b20-temperature-sensor.md`](../../references/ds18b20-temperature-sensor.md), [`references/dht22-humidity-sensor.md`](../../references/dht22-humidity-sensor.md)
