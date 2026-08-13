#include "ClimateControl.h"
#include <math.h>

float ClimateControl::coldestValidC(const float* temps, uint8_t n) {
  // Start above the validity window so the first valid reading wins, and
  // the "none valid" case ends up outside the window (invalid sentinel).
  float coldest = kTempValidHiC + 1.0f;
  for (uint8_t i = 0; i < n; ++i) {
    float t = temps[i];
    if (t > kTempValidLoC && t < kTempValidHiC && t < coldest) coldest = t;
  }
  return coldest > kTempValidHiC ? -100.0f : coldest;
}

float ClimateControl::dewPointC(float tempC, float humidityPct) {
  // Guard against invalid humidity (log(0) -> NaN).
  if (humidityPct <= 0.0f || humidityPct > 100.0f || isnan(humidityPct)) {
    return -100.0f;                      // "no indoor dew point"
  }
  // Magnus formula (valid ~0..60 C)
  const float a = 17.62f;
  const float b = 243.12f;
  float alpha = (a * tempC) / (b + tempC) + logf(humidityPct / 100.0f);
  return (b * alpha) / (a - alpha);
}

ControlDecision ClimateControl::decidePumps(const ControlInputs& in,
                                            const ControlParams& p,
                                            bool currentlyOn) {
  ControlDecision d;

  // Reference dew point = the HIGHER of outdoor (app weather) and indoor
  // (DHT22) -> most conservative for condensation protection. Indoor is
  // skipped until valid DHT22 telemetry arrives (protects boot-time NaN).
  d.refDewPointC = fmaxf(in.outdoorDewPointC,
                         dewPointC(in.indoorTempC, in.indoorHumidityPct));
  d.waterFloorC  = d.refDewPointC + p.dewPointMarginC;

  d.weatherDemand = in.outdoorTempC > p.weatherCoolTempC;
  // Demand from the indoor AIR temperature (DHT22) - the monitor's DS18B20s
  // sit on the water pipes above the ceiling and do not read room air.
  d.sensorDemand  = in.indoorTempC > p.comfortSetpointC;

  bool wantCooling = d.weatherDemand && d.sensorDemand;

  // Coldest measured water surface = min of the ceiling pipes (monitor) and
  // the chiller tank - the true anti-condensation point. Invalid readings
  // are ignored; with none valid the floor cannot be verified -> fail safe.
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
    // Stay on until the coldest surface reaches the floor
    // (anti-condensation override).
    d.pumpsOn = coldest > d.waterFloorC;
  } else {
    // Turn on only when safely above the floor (plus hysteresis).
    d.pumpsOn = coldest > d.waterFloorC + p.hysteresisC;
  }
  return d;
}
