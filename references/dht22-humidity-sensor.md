# DHT22 (AM2302) Humidity & Temperature Sensor

> Used on the dehumidifier board to measure indoor temperature + relative
> humidity — the gateway converts these to an indoor dew point.

## Key facts

- Measures relative humidity **0–100 %RH (±2 %)** and temperature −40..+80 °C
  (±0.5 °C).
- Single-wire digital protocol; one data pin + a **10 kΩ pull-up** to 3V3.
- Slow sampling: **max one read every ~2 s** — do not poll faster.
- Returns `NAN` when the read fails (timing/pin issues).
- Library: **DHT sensor library** (by Adafruit) in the Arduino Library Manager.

```cpp
#include <DHT.h>
DHT dht(PIN_DHT22, DHT22);

dht.begin();
float t = dht.readTemperature();   // °C
float h = dht.readHumidity();      // %RH
```

## How it is used here

- `HumiditySensor` module wraps the DHT library (dehumidifier board only).
- The 55 %RH setpoint control reads this sensor directly; the reading is also
  sent to the gateway over ESP-NOW for the dew-point computation.

## Links

- Datasheet: <https://www.sparkfun.com/datasheets/Sensors/Temperature/DHT22.pdf>
- DHT sensor library: <https://github.com/adafruit/DHT-sensor-library>
- RNT — DHT11/DHT22 with ESP32: <https://randomnerdtutorials.com/esp32-dht11-dht22-temperature-humidity-sensor-arduino-ide/>
