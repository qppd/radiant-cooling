# Power Supply — 220V AC → 5V DC (3A, one per board)

> Every ESP32 board is powered by its **own** AC-DC power supply:
> **220 V AC → 5 V DC, 3 A**. No shared rail between boards — each board is
> fully isolated from the others.

## Specification

| Parameter | Value |
| --------- | ----- |
| Input     | 220 V AC (mains) |
| Output    | 5 V DC, 3 A (15 W) |
| Quantity  | 3× — one per board |
| Feed      | 5 V → board **5V/VIN** pin → onboard 3.3 V regulator → ESP32 + sensors |

## Wiring notes

- **L (live) / N (neutral)** go to the PSU input; **PE (earth)** to the
  enclosure/chassis if the PSU provides it.
- Add a **fuse (e.g. 2 A)** on the live input for protection.
- PSU **5 V +** → board `5V`/`VIN`; PSU **GND −** → board `GND`.
- The SSR-switched loads (water pumps, dehumidifier) are on **separate mains
  circuits** — the 5 V supplies only power the logic boards and sensors.
- One shared 5 V rail across boards is **not** used (keeps grounds isolated
  per board).

## Safety

- **Warning:** 220 V mains work requires proper care: fuse the input, use an
  insulated enclosure, and keep all mains wiring away from the low-voltage
  side.
- Never connect sensors or SSR inputs to the 5 V pin; GPIOs are 3.3 V.
- The SSR isolates the load side — always switch the **load** with the SSR,
  never wire the pumps/dehumidifier through the board.

## How it is used here

- One supply feeds `RadiantCoolingMonitor`, one feeds `WaterChillerController`,
  one feeds `DehumidifierController` (see `docs/diagrams/block-diagram.md`).
- Power-on sequences are independent — a fault on one board never affects the
  others.

## Links

- AC-DC power supply basics: <https://en.wikipedia.org/wiki/AC%E2%80%93DC_adapters>
- ESP32 power requirements: <https://docs.espressif.com/projects/esp32/en/latest/_images/esp32_pinout.png>
