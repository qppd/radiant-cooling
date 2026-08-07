#include "ClimateControl.h"
#include <math.h>

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

  // Reference dew point = the HIGHER of outdoor (weather) and indoor (DHT22)
  // -> most conservative for condensation protection. Indoor is skipped
  // until valid DHT22 telemetry arrives (protects against boot-time NaN).
  d.refDewPointC = fmaxf(in.outdoorDewPointC,
                         dewPointC(in.indoorTempC, in.indoorHumidityPct));
  d.waterFloorC  = d.refDewPointC + p.dewPointMarginC;

  d.weatherDemand = in.outdoorTempC > p.weatherCoolTempC;
  d.sensorDemand  = in.hottestRoomC > p.comfortSetpointC;

  bool wantCooling = d.weatherDemand && d.sensorDemand;

  if (currentlyOn) {
    // Stay on until the water floor is reached (anti-condensation override).
    d.pumpsOn = wantCooling && (in.waterTempC > d.waterFloorC);
  } else {
    // Turn on only when safely above the floor (plus hysteresis).
    d.pumpsOn = wantCooling && (in.waterTempC > d.waterFloorC + p.hysteresisC);
  }
  return d;
}
