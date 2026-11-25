#pragma once
#include <Arduino.h>

static const float kTempValidLoC = -90.0f;
static const float kTempValidHiC =  60.0f;

struct ControlInputs {
  // Outdoor values are only usable while weatherValid is true. They default to
  // the "invalid" sentinel so a missing weather feed can never masquerade as
  // a comfortable 0 C outdoor reading.
  bool  weatherValid     = false;
  float outdoorTempC     = -100.0f;
  float outdoorDewPointC = -100.0f;
  float indoorTempC      = 0.0f;
  float indoorHumidityPct = 0.0f;
  float supplyTempC      = 0.0f;
  float returnTempC      = 0.0f;
  float coldestPipeC     = -100.0f;
  float waterTempC       = 0.0f;
};

struct ControlParams {
  float comfortSetpointC = 24.0f;
  float dewPointMarginC  = 2.0f;
  float weatherCoolTempC = 28.0f;
  float hysteresisC      = 1.0f;
};

struct ControlDecision {
  bool  pumpsOn = false;
  float refDewPointC = -100.0f;
  float waterFloorC = -100.0f;
  bool  weatherDemand = false;
  bool  sensorDemand = false;
  // Diagnostics for telemetry / serial status.
  bool  sensorOnly = false;       // weather feed unusable -> indoor sensors drive demand
  bool  dewPointKnown = false;    // at least one valid dew point reference
  bool  coldPointKnown = false;   // at least one valid pipe/tank temperature
};

class ClimateControl {
public:
  static float dewPointC(float tempC, float humidityPct);

  static float coldestValidC(const float* temps, uint8_t n);

  static ControlDecision decidePumps(const ControlInputs& in,
                                     const ControlParams& p,
                                     bool currentlyOn);
};
