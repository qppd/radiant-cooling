#pragma once
#include <Arduino.h>

static const float kTempValidLoC = -90.0f;
static const float kTempValidHiC =  60.0f;

struct ControlInputs {
  float outdoorTempC     = 0.0f;
  float outdoorDewPointC = 0.0f;
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
  bool  pumpsOn;
  float refDewPointC;
  float waterFloorC;
  bool  weatherDemand;
  bool  sensorDemand;
};

class ClimateControl {
public:
  static float dewPointC(float tempC, float humidityPct);

  static float coldestValidC(const float* temps, uint8_t n);

  static ControlDecision decidePumps(const ControlInputs& in,
                                     const ControlParams& p,
                                     bool currentlyOn);
};
