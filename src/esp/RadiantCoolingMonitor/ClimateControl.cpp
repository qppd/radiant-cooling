#include "ClimateControl.h"
#include <math.h>

float ClimateControl::coldestValidC(const float* temps, uint8_t n) {
  float coldest = kTempValidHiC + 1.0f;
  for (uint8_t i = 0; i < n; ++i) {
    float t = temps[i];
    if (t > kTempValidLoC && t < kTempValidHiC && t < coldest) coldest = t;
  }
  return coldest > kTempValidHiC ? -100.0f : coldest;
}

float ClimateControl::dewPointC(float tempC, float humidityPct) {
  if (humidityPct <= 0.0f || humidityPct > 100.0f || isnan(humidityPct)) {
    return -100.0f;
  }
  const float a = 17.62f;
  const float b = 243.12f;
  float alpha = (a * tempC) / (b + tempC) + logf(humidityPct / 100.0f);
  return (b * alpha) / (a - alpha);
}

ControlDecision ClimateControl::decidePumps(const ControlInputs& in,
                                            const ControlParams& p,
                                            bool currentlyOn) {
  ControlDecision d;

  d.refDewPointC = fmaxf(in.outdoorDewPointC,
                         dewPointC(in.indoorTempC, in.indoorHumidityPct));
  d.waterFloorC  = d.refDewPointC + p.dewPointMarginC;

  d.weatherDemand = in.outdoorTempC > p.weatherCoolTempC;
  d.sensorDemand  = in.indoorTempC > p.comfortSetpointC;

  bool wantCooling = d.weatherDemand && d.sensorDemand;

  float coldest = 1e9f;
  uint8_t valid = 0;
  if (in.coldestPipeC > kTempValidLoC && in.coldestPipeC < kTempValidHiC) {
    coldest = fminf(coldest, in.coldestPipeC);
    ++valid;
  }
  if (in.waterTempC > kTempValidLoC && in.waterTempC < kTempValidHiC) {
    coldest = fminf(coldest, in.waterTempC);
    ++valid;
  }

  if (!wantCooling || valid == 0) {
    d.pumpsOn = false;
    return d;
  }

  if (currentlyOn) {
    d.pumpsOn = coldest > d.waterFloorC;
  } else {
    d.pumpsOn = coldest > d.waterFloorC + p.hysteresisC;
  }
  return d;
}
