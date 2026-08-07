/*
 * ClimateControl.h - control module
 *
 * The gateway-side control computation for the water chiller pumps:
 *   - weather-driven demand   (outdoor temp from WeatherAPI)
 *   - sensor demand           (hottest room temp from the 6x DS18B20)
 *   - anti-condensation floor (water temp must stay above dew point +
 *     safety margin; dew point = higher of outdoor and indoor)
 *
 * The decision is a draft - tune the parameters and thresholds as the
 * system is commissioned (see docs/diagrams/flow-chart.md).
 */
#pragma once
#include <Arduino.h>

struct ControlInputs {
  float outdoorTempC    = 0.0f;   // WeatherAPI
  float outdoorDewPointC = 0.0f;  // WeatherAPI
  float indoorTempC     = 0.0f;   // DHT22 (dehumidifier telemetry)
  float indoorHumidityPct = 0.0f; // DHT22 (dehumidifier telemetry)
  float waterTempC      = 0.0f;   // DS18B20 (chiller telemetry)
  float hottestRoomC    = 0.0f;   // max of room sensors (monitor DS18B20)
};

struct ControlParams {
  float comfortSetpointC = 24.0f;  // rooms above this -> cooling demand (C)
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

  // Combines weather + sensors + condensation protection into a pump on/off
  // decision. `currentlyOn` provides the hysteresis baseline.
  static ControlDecision decidePumps(const ControlInputs& in,
                                     const ControlParams& p,
                                     bool currentlyOn);
};
