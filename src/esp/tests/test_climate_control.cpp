/*
 * test_climate_control.cpp - unit tests for the ClimateControl module.
 *
 * Compiles ClimateControl.{h,cpp} on a host machine (using the mock
 * Arduino.h in this folder) and verifies:
 *   - Magnus-formula dew point        (ClimateControl::dewPointC)
 *   - pump decision logic             (ClimateControl::decidePumps):
 *       weather demand, sensor demand, condensation protection,
 *       switching hysteresis, and the reference dew point selection
 *
 * No test framework is required - plain asserts with a tiny harness.
 *
 * Run:  ./run_tests.sh   (requires g++)
 */
#include <stdio.h>
#include <math.h>

#include "ClimateControl.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond) \
  do { \
    ++g_checks; \
    if (!(cond)) { \
      ++g_failures; \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    } \
  } while (0)

#define CHECK_CLOSE(actual, expected, tol) \
  do { \
    float _a = (actual), _e = (expected); \
    ++g_checks; \
    if (fabsf(_a - _e) > (tol)) { \
      ++g_failures; \
      printf("FAIL %s:%d  %s = %.3f, expected %.3f +/- %.3f\n", \
             __FILE__, __LINE__, #actual, (double)_a, (double)_e, (double)(tol)); \
    } \
  } while (0)

// Default control parameters used by the decision tests (same defaults as
// the ControlParams struct).
static ControlParams defaultParams() {
  ControlParams p;
  p.comfortSetpointC = 24.0f;   // rooms above this -> cooling demand
  p.dewPointMarginC  = 2.0f;    // water floor = dew point + margin
  p.weatherCoolTempC = 28.0f;   // outdoor above this -> weather demand
  p.hysteresisC      = 1.0f;
  return p;
}

// ---------------------------------------------------------------------------
// dewPointC - Magnus formula
// ---------------------------------------------------------------------------
static void testDewPointKnownValues() {
  // Reference values computed with the Magnus formula (a=17.62, b=243.12).
  CHECK_CLOSE(ClimateControl::dewPointC(20.0f, 50.0f),  9.255f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(25.0f, 100.0f), 25.000f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(10.0f, 90.0f),  8.434f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(24.0f, 55.0f),  14.400f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(30.0f, 40.0f),  14.925f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(18.0f, 65.0f),  11.319f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(22.0f, 45.0f),  9.515f, 0.1f);

  // 100% RH: dew point equals air temperature.
  CHECK_CLOSE(ClimateControl::dewPointC(0.0f, 100.0f), 0.0f, 0.05f);
  CHECK_CLOSE(ClimateControl::dewPointC(15.0f, 100.0f), 15.0f, 0.05f);
}

static void testDewPointGuards() {
  // Invalid humidity -> "no indoor dew point" sentinel (-100).
  CHECK(ClimateControl::dewPointC(20.0f, 0.0f) == -100.0f);    // RH 0
  CHECK(ClimateControl::dewPointC(20.0f, -5.0f) == -100.0f);   // RH < 0
  CHECK(ClimateControl::dewPointC(20.0f, 101.0f) == -100.0f);  // RH > 100
  CHECK(ClimateControl::dewPointC(20.0f, NAN) == -100.0f);     // NaN humidity
}

// ---------------------------------------------------------------------------
// decidePumps - decision logic
// ---------------------------------------------------------------------------

// Helper: inputs that satisfy BOTH demands with a safe water temperature.
static ControlInputs warmAndComfortableInputs() {
  ControlInputs in;
  in.outdoorTempC      = 32.0f;   // > 28 -> weather demand
  in.outdoorDewPointC  = 20.0f;   // floor = 22
  in.indoorTempC       = 24.0f;   // indoor dew point ~14.4 (< outdoor)
  in.indoorHumidityPct = 55.0f;
  in.waterTempC        = 25.0f;   // > 22 + 1 hysteresis
  in.hottestRoomC      = 26.0f;   // > 24 -> sensor demand
  return in;
}

static void testDecidePumpsOffToOn() {
  ControlDecision d =
      ClimateControl::decidePumps(warmAndComfortableInputs(), defaultParams(), false);

  CHECK(d.weatherDemand == true);
  CHECK(d.sensorDemand == true);
  CHECK_CLOSE(d.refDewPointC, 20.0f, 0.1f);   // max(outdoor 20, indoor ~14.4)
  CHECK_CLOSE(d.waterFloorC, 22.0f, 0.1f);    // 20 + margin 2
  CHECK(d.pumpsOn == true);                   // water 25 > floor 22 + hyst 1
}

static void testDecidePumpsNoWeatherDemand() {
  ControlInputs in = warmAndComfortableInputs();
  in.outdoorTempC = 25.0f;                    // <= 28 -> no weather demand
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK(d.weatherDemand == false);
  CHECK(d.sensorDemand == true);
  CHECK(d.pumpsOn == false);                  // wantCooling is false
}

static void testDecidePumpsNoSensorDemand() {
  ControlInputs in = warmAndComfortableInputs();
  in.hottestRoomC = 22.0f;                    // <= 24 -> no sensor demand
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK(d.weatherDemand == true);
  CHECK(d.sensorDemand == false);
  CHECK(d.pumpsOn == false);                  // wantCooling is false
}

static void testCondensationOverride() {
  // Pump is ON but the water temperature has dropped below the floor
  // (dew point + margin) -> must turn OFF to avoid condensation.
  ControlInputs in = warmAndComfortableInputs();
  in.outdoorDewPointC = 22.0f;                // floor = 24
  in.waterTempC       = 23.0f;                // below the floor
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), true);

  CHECK(d.weatherDemand == true);
  CHECK(d.sensorDemand == true);
  CHECK_CLOSE(d.waterFloorC, 24.0f, 0.1f);
  CHECK(d.pumpsOn == false);                  // condensation override wins
}

static void testSwitchingHysteresis() {
  // floor = 22, hysteresis = 1:
  //   off -> on requires water > 23
  //   on  -> off requires water <= 22
  // water = 22.5 sits in the hysteresis band.
  ControlInputs in = warmAndComfortableInputs();
  in.waterTempC = 22.5f;

  ControlDecision off = ClimateControl::decidePumps(in, defaultParams(), false);
  CHECK(off.pumpsOn == false);                // 22.5 not > 23

  ControlDecision on = ClimateControl::decidePumps(in, defaultParams(), true);
  CHECK(on.pumpsOn == true);                  // 22.5 > 22
}

static void testIndoorDewPointReference() {
  // Indoor dew point (higher) is used as the reference when it exceeds the
  // outdoor dew point.
  ControlInputs in = warmAndComfortableInputs();
  in.outdoorDewPointC = 10.0f;                // outdoor dew point 10
  in.indoorTempC      = 24.0f;                // indoor dew point ~14.4
  in.indoorHumidityPct = 55.0f;
  in.waterTempC       = 18.0f;                // > floor 16.4 + hyst 1
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK_CLOSE(d.refDewPointC, 14.4f, 0.2f);   // max(10, ~14.4) = indoor
  CHECK_CLOSE(d.waterFloorC, 16.4f, 0.2f);    // 14.4 + margin 2
  CHECK(d.pumpsOn == true);
}

static void testNoIndoorDewPointFallback() {
  // Invalid indoor humidity -> indoor dew point sentinel (-100); the
  // outdoor dew point must be used as the reference instead.
  ControlInputs in = warmAndComfortableInputs();
  in.indoorHumidityPct = 0.0f;                // invalid
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK_CLOSE(d.refDewPointC, 20.0f, 0.1f);   // falls back to outdoor
}

// ---------------------------------------------------------------------------
int main() {
  testDewPointKnownValues();
  testDewPointGuards();
  testDecidePumpsOffToOn();
  testDecidePumpsNoWeatherDemand();
  testDecidePumpsNoSensorDemand();
  testCondensationOverride();
  testSwitchingHysteresis();
  testIndoorDewPointReference();
  testNoIndoorDewPointFallback();

  printf("\n%d check(s), %d failure(s)\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
