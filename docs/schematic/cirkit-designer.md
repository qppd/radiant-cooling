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
| `RadiantCoolingMonitor`      | ESP32 (gateway) + 4× DS18B20 (one 1-Wire bus each) + WiFi reset button |
| `WaterChillerController`     | ESP32 (peer) + 2× DS18B20 (tank out/in) + 2× SSR → compressor + pump |
| `DehumidifierController`     | ESP32 (peer) + 1× DHT22 + 1× SSR → dehumidifier      |

Each board sits on its own breadboard; the three are wired independently
(one 220 V → 5 V / 3 A supply per board, no shared rail) and only talk to
each other over the radio (ESP-NOW), so there are no inter-board wires in
the schematic.

## Sensor placement (physical installation)

- **4× DS18B20 (gateway, monitor)** — ceiling sensors mounted on the pipes
  with a 3D-printed mount for a 1/2 inch PVC T-connector. Each sensor has
  its **own 1-Wire bus**: GPIO 23, 21, 18 and 19 (one 4.7 kΩ pull-up to 3V3
  per bus). Sensors 1-2 feed the supply/return telemetry.
- **2× DS18B20 (chiller)** — tank water temperature: **outgoing** water
  leaving the tank (GPIO 5) and **ingoing** water returning to the tank
  (GPIO 18). Feed `outgoing_temp_c`/`ingoing_temp_c` telemetry, the
  `water_temp_c` value (coldest of the two) and the chiller fail-safe.
- **1× DHT22 (dehumidifier)** — indoor temperature + relative humidity
  (GPIO 25, 10 kΩ pull-up). The only humidity source → indoor dew point for
  the anti-condensation floor.

## Wiring notes

- **1-Wire (DS18B20):** each sensor in this project has its own data line
  (one sensor per GPIO); 4.7 kΩ pull-up from each data line to 3V3. Use an
  external 3V3 supply, not parasite power.
- **DHT22:** 10 kΩ pull-up on the data line.
- **SSR modules:** 3–32 V DC input — drive directly from a 3.3 V GPIO;
  buffer with a transistor/MOSFET if the module needs more drive. Always
  switch the mains load with the SSR; never wire pumps/dehumidifier through
  the board.
- **Power:** each board fed by its own **220 V AC → 5 V DC, 3 A** supply into
  `5V`/`VIN`. No shared rail between boards.
- **WiFi reset button (gateway):** GPIO 25 → GND, momentary; hold 3 s to
  erase WiFi credentials.

See [`pin-map.md`](pin-map.md) for the full pin-by-pin rationale (boot,
flash, UART0 and ADC2 avoidance on the 38-pin ESP32) and
[`references/README.md`](../../references/README.md) for the list of
electrical reference documents.

## Offline diagram

An interactive **3D wiring diagram** lives in this folder (no Cirkit
account needed):

- **`wiring-diagram.html`** — an interactive **Cirkit-style** flat 2D
  wiring diagram (Three.js, orthographic top view with dot grid, thin
  color-coded wires with junction dots). Pan/zoom to explore; hover/click
  a component to highlight its wires; toggles show/hide power, 1-Wire,
  GPIO and ESP-NOW groups. Open it in any browser (needs internet for the
  Three.js CDN).

Pin assignments in the schematic and `PINS_CONFIG.h` are kept in sync;
update both together when rewiring a board.
