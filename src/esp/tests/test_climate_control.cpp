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

static ControlParams defaultParams() {
  ControlParams p;
  p.comfortSetpointC = 24.0f;
  p.dewPointMarginC  = 2.0f;
  p.weatherCoolTempC = 28.0f;
  p.hysteresisC      = 1.0f;
  return p;
}

static void testDewPointKnownValues() {
  CHECK_CLOSE(ClimateControl::dewPointC(20.0f, 50.0f),  9.255f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(25.0f, 100.0f), 25.000f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(10.0f, 90.0f),  8.434f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(24.0f, 55.0f),  14.400f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(30.0f, 40.0f),  14.925f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(18.0f, 65.0f),  11.319f, 0.1f);
  CHECK_CLOSE(ClimateControl::dewPointC(22.0f, 45.0f),  9.515f, 0.1f);

  CHECK_CLOSE(ClimateControl::dewPointC(0.0f, 100.0f), 0.0f, 0.05f);
  CHECK_CLOSE(ClimateControl::dewPointC(15.0f, 100.0f), 15.0f, 0.05f);
}

static void testDewPointGuards() {
  CHECK(ClimateControl::dewPointC(20.0f, 0.0f) == -100.0f);
  CHECK(ClimateControl::dewPointC(20.0f, -5.0f) == -100.0f);
  CHECK(ClimateControl::dewPointC(20.0f, 101.0f) == -100.0f);
  CHECK(ClimateControl::dewPointC(20.0f, NAN) == -100.0f);
}

static void testColdestValidC() {
  float allInvalid[4] = { -127.0f, 85.0f, -100.0f, NAN };
  CHECK(ClimateControl::coldestValidC(allInvalid, 4) == -100.0f);

  float mixed[4] = { 25.0f, 85.0f, 18.5f, -127.0f };
  CHECK_CLOSE(ClimateControl::coldestValidC(mixed, 4), 18.5f, 0.001f);

  float withGarbage[3] = { NAN, 22.0f, 19.0f };
  CHECK_CLOSE(ClimateControl::coldestValidC(withGarbage, 3), 19.0f, 0.001f);
}


static ControlInputs warmAndComfortableInputs() {
  ControlInputs in;
  in.outdoorTempC      = 32.0f;
  in.outdoorDewPointC  = 20.0f;
  in.indoorTempC       = 26.0f;
  in.indoorHumidityPct = 55.0f;
  in.supplyTempC       = 24.0f;
  in.returnTempC       = 25.0f;
  in.coldestPipeC      = 24.0f;
  in.waterTempC        = 25.0f;
  return in;
}

static void testDecidePumpsOffToOn() {
  ControlDecision d =
      ClimateControl::decidePumps(warmAndComfortableInputs(), defaultParams(), false);

  CHECK(d.weatherDemand == true);
  CHECK(d.sensorDemand == true);
  CHECK_CLOSE(d.refDewPointC, 20.0f, 0.1f);
  CHECK_CLOSE(d.waterFloorC, 22.0f, 0.1f);
  CHECK(d.pumpsOn == true);
}

static void testDecidePumpsNoWeatherDemand() {
  ControlInputs in = warmAndComfortableInputs();
  in.outdoorTempC = 25.0f;
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK(d.weatherDemand == false);
  CHECK(d.sensorDemand == true);
  CHECK(d.pumpsOn == false);
}

static void testDecidePumpsNoSensorDemand() {
  ControlInputs in = warmAndComfortableInputs();
  in.indoorTempC = 22.0f;
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK(d.weatherDemand == true);
  CHECK(d.sensorDemand == false);
  CHECK(d.pumpsOn == false);
}

static void testCondensationOverride() {
  ControlInputs in = warmAndComfortableInputs();
  in.outdoorDewPointC = 22.0f;
  in.coldestPipeC     = 21.0f;
  in.waterTempC       = 25.0f;
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), true);

  CHECK(d.weatherDemand == true);
  CHECK(d.sensorDemand == true);
  CHECK_CLOSE(d.waterFloorC, 24.0f, 0.1f);
  CHECK(d.pumpsOn == false);
}

static void testColdestSurfaceWins() {
  ControlInputs in = warmAndComfortableInputs();
  in.outdoorDewPointC = 22.0f;
  in.coldestPipeC     = 23.0f;
  in.waterTempC       = 25.0f;
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), true);

  CHECK_CLOSE(d.waterFloorC, 24.0f, 0.1f);
  CHECK(d.pumpsOn == false);
}

static void testSwitchingHysteresis() {
  ControlInputs in = warmAndComfortableInputs();
  in.coldestPipeC = 22.5f;
  in.waterTempC   = 25.0f;

  ControlDecision off = ClimateControl::decidePumps(in, defaultParams(), false);
  CHECK(off.pumpsOn == false);

  ControlDecision on = ClimateControl::decidePumps(in, defaultParams(), true);
  CHECK(on.pumpsOn == true);
}

static void testIndoorDewPointReference() {
  ControlInputs in = warmAndComfortableInputs();
  in.outdoorDewPointC = 10.0f;
  in.coldestPipeC     = 21.0f;
  in.waterTempC       = 25.0f;
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK_CLOSE(d.refDewPointC, 16.3f, 0.2f);
  CHECK_CLOSE(d.waterFloorC, 18.3f, 0.2f);
  CHECK(d.pumpsOn == true);
}

static void testNoIndoorDewPointFallback() {
  ControlInputs in = warmAndComfortableInputs();
  in.indoorHumidityPct = 0.0f;
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK_CLOSE(d.refDewPointC, 20.0f, 0.1f);
}

static void testNoWaterTempFailSafe() {
  ControlInputs in = warmAndComfortableInputs();
  in.coldestPipeC = -100.0f;
  in.waterTempC   = -127.0f;
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK(d.weatherDemand == true);
  CHECK(d.sensorDemand == true);
  CHECK(d.pumpsOn == false);
}

static void testTankOnlyFloorCheck() {
  ControlInputs in = warmAndComfortableInputs();
  in.coldestPipeC = -100.0f;
  in.waterTempC   = 25.0f;
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK(d.pumpsOn == true);
}

static void testPipesOnlyFloorCheck() {
  ControlInputs in = warmAndComfortableInputs();
  in.waterTempC = -127.0f;
  ControlDecision d =
      ClimateControl::decidePumps(in, defaultParams(), false);

  CHECK(d.pumpsOn == true);
}

int main() {
  testDewPointKnownValues();
  testDewPointGuards();
  testColdestValidC();
  testDecidePumpsOffToOn();
  testDecidePumpsNoWeatherDemand();
  testDecidePumpsNoSensorDemand();
  testCondensationOverride();
  testColdestSurfaceWins();
  testSwitchingHysteresis();
  testIndoorDewPointReference();
  testNoIndoorDewPointFallback();
  testNoWaterTempFailSafe();
  testTankOnlyFloorCheck();
  testPipesOnlyFloorCheck();

  printf("\n%d check(s), %d failure(s)\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
