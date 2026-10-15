#pragma once
#include <Arduino.h>

// Database path helpers for Radiant Cooling
class RadiantFirebaseConfig {
public:
  // RTDB paths
  static const char* getConfigPath();
  static const char* getConfigControlParamsPath();
  static const char* getConfigDhPath();
  static const char* getConfigWeatherKeyPath();
  static const char* getTelemetryBasePath();
  static const char* getStateBasePath();
  static const char* getHeartbeatPath();
  static const char* getDevicesBasePath();

  // Control stream path (realtime commands from app/dashboard)
  static const char* getControlPath();
};
