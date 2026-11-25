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

static bool validTemp(float t) {
  return t > kTempValidLoC && t < kTempValidHiC;
}

static bool validHumidity(float h) {
  return h > 0.0f && h <= 100.0f && !isnan(h);
}

ControlDecision ClimateControl::decidePumps(const ControlInputs& in,
                                            const ControlParams& p,
                                            bool currentlyOn) {
  ControlDecision d;

  // Build the reference dew point from whichever sources are actually valid.
  // Outdoor values are ignored unless the weather feed is marked valid, so an
  // offline gateway falls back to the indoor DHT22 dew point and never treats
  // the -100 C sentinels (or the old 0 C defaults) as real readings.
  const bool indoorTempOk     = validTemp(in.indoorTempC);
  const bool indoorHumidityOk = validHumidity(in.indoorHumidityPct);
  const bool outdoorDewOk     = in.weatherValid && validTemp(in.outdoorDewPointC);

  bool dewPointKnown = false;
  float refDewPointC = -100.0f;

  if (outdoorDewOk) {
    refDewPointC = in.outdoorDewPointC;
    dewPointKnown = true;
  }
  if (indoorTempOk && indoorHumidityOk) {
    float indoorDew = dewPointC(in.indoorTempC, in.indoorHumidityPct);
    if (validTemp(indoorDew)) {
      refDewPointC = dewPointKnown ? fmaxf(refDewPointC, indoorDew) : indoorDew;
      dewPointKnown = true;
    }
  }

  d.dewPointKnown = dewPointKnown;
  d.refDewPointC  = dewPointKnown ? refDewPointC : -100.0f;
  d.waterFloorC   = dewPointKnown ? refDewPointC + p.dewPointMarginC : -100.0f;
  d.sensorOnly    = !in.weatherValid;

  d.sensorDemand = indoorTempOk && in.indoorTempC > p.comfortSetpointC;

  // Online: outdoor temperature gates cooling. Offline: we have no outdoor
  // reading, so indoor demand alone drives the pumps (sensor-only mode).
  d.weatherDemand = in.weatherValid
                        ? (validTemp(in.outdoorTempC) &&
                           in.outdoorTempC > p.weatherCoolTempC)
                        : true;

  // No dew point reference -> we cannot prove the pipes are above the
  // condensation floor, so fail safe with the pumps off.
  bool wantCooling = d.sensorDemand && d.weatherDemand && d.dewPointKnown;

  float coldest = 1e9f;
  uint8_t valid = 0;
  if (validTemp(in.coldestPipeC)) {
    coldest = fminf(coldest, in.coldestPipeC);
    ++valid;
  }
  if (validTemp(in.waterTempC)) {
    coldest = fminf(coldest, in.waterTempC);
    ++valid;
  }
  d.coldPointKnown = valid > 0;

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
