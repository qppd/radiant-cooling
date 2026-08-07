# Solid State Relay (SSR)

> Used to switch the two water pumps (chiller board, 2×) and the dehumidifier
> (dehumidifier board, 1×). SSRs are driven by the ESP32 GPIOs and isolate
> the low-voltage logic from the mains/load side.

## Key facts

- **DC-input AC-output SSRs** are the common hobby type: input accepts
  **3–32 V DC**, output switches the mains load.
- **Zero-crossing** models switch near the AC zero point → less EMI, but the
  response adds up to half a mains cycle; good for pumps/heaters.
- The ESP32's **3.3 V GPIO can usually drive the input directly**, but always
  check the module's input spec — if it needs more current, buffer with a
  transistor/MOSFET.
- Always switch the **load side** (pump / dehumidifier) with the SSR — never
  power them from the board's pins.

## How it is used here

- `SsrOutput` module wraps a single SSR as a digital output (`on()` / `off()`,
  starts OFF in `begin()`).
- Pins: chiller pumps → GPIO 19, 21; dehumidifier → GPIO 23 (boot/Wi-Fi-safe,
  see `docs/schematic/pin-map.md`).

## Links

- Guide — SSR basics: <https://www.electronics-tutorials.ws/blog/solid-state-relay.html>
- RNT — ESP32 control a relay (concept applies to SSRs): <https://randomnerdtutorials.com/esp32-relay-module-ac-control/>
