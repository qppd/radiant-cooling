#include "SettingsStore.h"

static const char* kNamespace = "radiant";

void SettingsStore::begin() {
  _ready = _prefs.begin(kNamespace, false);
  if (!_ready) {
    Serial.println("[settings] NVS unavailable - using defaults");
  }
}

bool SettingsStore::load(ControlParams& params, DhConfig& dh) {
  if (!_ready) return false;

  bool any = false;
  if (_prefs.isKey("comfort")) {
    params.comfortSetpointC = _prefs.getFloat("comfort", params.comfortSetpointC);
    any = true;
  }
  if (_prefs.isKey("margin")) {
    params.dewPointMarginC = _prefs.getFloat("margin", params.dewPointMarginC);
    any = true;
  }
  if (_prefs.isKey("weather_cool")) {
    params.weatherCoolTempC = _prefs.getFloat("weather_cool", params.weatherCoolTempC);
    any = true;
  }
  if (_prefs.isKey("dh_set")) {
    dh.humiditySetpointPct = _prefs.getFloat("dh_set", dh.humiditySetpointPct);
    any = true;
  }
  if (_prefs.isKey("dh_db")) {
    dh.humidityDeadbandPct = _prefs.getFloat("dh_db", dh.humidityDeadbandPct);
    any = true;
  }

  _params = params;
  _dh = dh;
  return any;
}

void SettingsStore::saveParams(const ControlParams& params) {
  if (!_ready) return;
  if (params.comfortSetpointC == _params.comfortSetpointC &&
      params.dewPointMarginC == _params.dewPointMarginC &&
      params.weatherCoolTempC == _params.weatherCoolTempC) {
    return;
  }

  _params = params;
  _prefs.putFloat("comfort", params.comfortSetpointC);
  _prefs.putFloat("margin", params.dewPointMarginC);
  _prefs.putFloat("weather_cool", params.weatherCoolTempC);
  Serial.printf("[settings] saved: setpoint=%.1f margin=%.1f weather_cool=%.1f\n",
                params.comfortSetpointC, params.dewPointMarginC,
                params.weatherCoolTempC);
}

void SettingsStore::saveDh(const DhConfig& dh) {
  if (!_ready) return;
  if (dh.humiditySetpointPct == _dh.humiditySetpointPct &&
      dh.humidityDeadbandPct == _dh.humidityDeadbandPct) {
    return;
  }

  _dh = dh;
  _prefs.putFloat("dh_set", dh.humiditySetpointPct);
  _prefs.putFloat("dh_db", dh.humidityDeadbandPct);
  Serial.printf("[settings] saved DH: setpoint=%.1f deadband=%.1f\n",
                dh.humiditySetpointPct, dh.humidityDeadbandPct);
}
