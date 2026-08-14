# Cirkit Designer Schematic

> Interactive wiring diagram for the radiant cooling system, drawn in
> **Cirkit Designer** (a free online circuit design tool). The schematic is
> versioned in the cloud; this page links to it and maps it to the firmware.

## Shareable link

<https://app.cirkitdesigner.com/project/5134caf3-4066-457c-ad06-4487625c0ef9>

Open in any browser — no login required to view. You can also duplicate the
project into your own workspace (Cirkit Designer → Project menu → Duplicate)
to edit or extend it.

## What it shows

The schematic models the three ESP32 boards and their wiring, matching the
layout in [`pin-map.md`](pin-map.md) and
[`docs/diagrams/block-diagram.md`](../diagrams/block-diagram.md):

| Board                        | Components in the schematic                          |
| ---------------------------- | ---------------------------------------------------- |
| `RadiantCoolingMonitor`      | ESP32 (gateway) + 6× DS18B20 on one 1-Wire bus       |
| `WaterChillerController`     | ESP32 (peer) + 1× DS18B20 (tank) + 2× SSR → pumps    |
| `DehumidifierController`     | ESP32 (peer) + 1× DHT22 + 1× SSR → dehumidifier      |

Each board sits on its own breadboard; the three are wired independently
(one 220 V → 5 V / 3 A supply per board, no shared rail) and only talk to
each other over the radio (ESP-NOW), so there are no inter-board wires in
the schematic.

## Sensor placement (physical installation)

- **6× DS18B20 (gateway, monitor)** — mounted on the chilled-water pipes
  **above the ceiling**: water **supply**, water **return**, and **4 pipe
  temperatures** along the run. All six share one 1-Wire bus (GPIO 18) with
  a 4.7 kΩ pull-up to 3V3.
- **1× DS18B20 (chiller)** — water temperature on the chiller tank (GPIO 22,
  4.7 kΩ pull-up). Feeds `water_temp_c` telemetry and the chiller fail-safe.
- **1× DHT22 (dehumidifier)** — indoor temperature + relative humidity
  (GPIO 32, 10 kΩ pull-up). The only humidity source → indoor dew point for
  the anti-condensation floor.

## Wiring notes

- **1-Wire (DS18B20):** one data line for all sensors on a board; 4.7 kΩ
  pull-up from the data line to 3V3. Use external 3V3 supply, not parasite
  power.
- **DHT22:** 10 kΩ pull-up on the data line.
- **SSR modules:** 3–32 V DC input — drive directly from a 3.3 V GPIO;
  buffer with a transistor/MOSFET if the module needs more drive. Always
  switch the mains load with the SSR; never wire pumps/dehumidifier through
  the board.
- **Power:** each board fed by its own **220 V AC → 5 V DC, 3 A** supply into
  `5V`/`VIN`. No shared rail between boards.
- **WiFi reset button (gateway):** GPIO 33 → GND, momentary; hold 3 s to
  erase WiFi credentials.

See [`pin-map.md`](pin-map.md) for the full pin-by-pin rationale (boot,
flash, UART0 and ADC2 avoidance on the 38-pin ESP32) and
[`references/README.md`](../../references/README.md) for the list of
electrical reference documents.

## Offline diagram

A rendered copy of the wiring lives in this folder (no Cirkit account
needed):

- **`wiring-diagram.png`** — the diagram as an image (1760×1080)
- `wiring-diagram.svg` — the editable source (regenerate the PNG from it)

Pin assignments in the schematic and `PINS_CONFIG.h` are kept in sync;
update both together when rewiring a board.
