/*
 * ClimateControl.h - control module
 *
 * The gateway-side control computation for the water chiller pumps:
 *   - weather-driven demand   (outdoor temp streamed from the app)
 *   - sensor demand           (indoor air temp from the DHT22)
 *   - anti-condensation floor (the COLDEST measured pipe/tank temperature
 *     must stay above dew point + margin; dew point = higher of outdoor
 *     and indoor)
 *
 * The decision is a draft - tune the parameters and thresholds as the
 * system is commissioned (see docs/diagrams/flow-chart.md).
 */
#pragma once
#include <Arduino.h>

// Validity window for water/pipe temperature readings (C). Readings outside
// it (disconnected -127, power-up glitch 85, NaN) are ignored by the
// anti-condensation logic.
static const float kTempValidLoC = -90.0f;
static const float kTempValidHiC =  60.0f;

struct ControlInputs {
  float outdoorTempC     = 0.0f;    // outdoor temp (app weather stream)
  float outdoorDewPointC = 0.0f;    // outdoor dew point (app weather stream)
  float indoorTempC      = 0.0f;    // indoor air temp - DHT22 (dehumidifier telemetry)
  float indoorHumidityPct = 0.0f;   // indoor RH - DHT22 (dehumidifier telemetry)
  float supplyTempC      = 0.0f;    // chilled-water supply (monitor DS18B20[0])
  float returnTempC      = 0.0f;    // chilled-water return (monitor DS18B20[1])
  float coldestPipeC     = -100.0f; // coldest monitor pipe reading (invalid until read)
  float waterTempC       = 0.0f;    // chiller tank - DS18B20 (chiller telemetry)
};

struct ControlParams {
  float comfortSetpointC = 24.0f;  // indoor air (DHT22) above this -> cooling demand (C)
  float dewPointMarginC  = 2.0f;   // water floor = dew point + margin (C)
  float weatherCoolTempC = 28.0f;  // outdoor temp above this -> weather demand (C)
  float hysteresisC      = 1.0f;   // switching hysteresis (C)
};

struct ControlDecision {
  bool  pumpsOn;
  float refDewPointC;   // higher of outdoor / indoor dew point
  float waterFloorC;    // refDewPointC + dewPointMarginC (anti-condensation)
  bool  weatherDemand;
  bool  sensorDemand;
};

class ClimateControl {
public:
  // Magnus-formula dew point from temperature + relative humidity.
  static float dewPointC(float tempC, float humidityPct);

  // Smallest VALID temperature from an array of readings (invalid = outside
  // the validity window, e.g. disconnected -127, glitch 85, NaN). Returns
  // -100.0f when none are valid.
  static float coldestValidC(const float* temps, uint8_t n);

  // Combines weather + sensors + condensation protection into a pump on/off
  // decision. The condensation floor is checked against the COLDEST of the
  // pipe sensors (monitor) and the chiller tank; invalid readings are
  // ignored, and if none are valid the pumps fail safe to OFF. `currentlyOn`
  // provides the hysteresis baseline.
  static ControlDecision decidePumps(const ControlInputs& in,
                                     const ControlParams& p,
                                     bool currentlyOn);
};
