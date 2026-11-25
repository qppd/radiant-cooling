#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "ClimateControl.h"

// Dehumidifier setpoints cached by the gateway. The gateway forwards these to
// the DH peer, so it must remember them across an offline reboot.
struct DhConfig {
  float humiditySetpointPct = 55.0f;
  float humidityDeadbandPct = 5.0f;
};

// Small NVS-backed store for the last settings received from Firebase. Without
// this an offline power cycle silently reverted the user's tuning to the
// compiled defaults. Pump state and manual/auto mode are deliberately NOT
// stored: after a reboot the gateway always returns to automatic control.
class SettingsStore {
public:
  void begin();
  bool load(ControlParams& params, DhConfig& dh);
  void saveParams(const ControlParams& params);
  void saveDh(const DhConfig& dh);

private:
  Preferences _prefs;
  bool _ready = false;
  ControlParams _params;
  DhConfig _dh;
};
