# DS18B20 Temperature Sensor (1-Wire)

> Digital temperature sensor used on the gateway (6×) and the chiller board
> (1×). All sensors share a single GPIO via the 1-Wire bus.

## Key facts

- ±0.5 °C accuracy (−10..+85 °C); 9–12 bit resolution; measures from −55 to
  +125 °C.
- **1-Wire bus:** every DS18B20 has a unique 64-bit ROM address, so **many
  sensors share one data pin** (addressed by index after enumeration).
- Requires a **4.7 kΩ pull-up** resistor from the data line to 3V3.
- ~750 ms conversion time at 12-bit resolution.
- Returns `-127.0` (`DEVICE_DISCONNECTED_C`) when a sensor is missing.

### Libraries (Arduino Library Manager)

- **OneWire** (bus protocol) + **DallasTemperature** (high-level API).

```cpp
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire oneWire(PIN);
DallasTemperature sensors(&oneWire);

sensors.begin();                        // enumerate devices
sensors.requestTemperatures();          // start conversion (async)
float t = sensors.getTempCByIndex(0);   // read sensor 0 after ~750 ms
```

## How it is used here

- `TemperatureSensor` module wraps the pair — the gateway constructs it with
  `count = 6`, the chiller with `count = 1` (same 1-Wire pin wiring).
- Room temperatures feed the pump-control computation (`ClimateControl`).

## Links

- Datasheet (Analog Devices): <https://www.analog.com/en/products/ds18b20.html>
- RNT — multiple DS18B20 with ESP32: <https://randomnerdtutorials.com/esp32-multiple-ds18b20-temperature-sensors/>
- DallasTemperature library: <https://github.com/milesburton/Arduino-Temperature-Control-Library>
