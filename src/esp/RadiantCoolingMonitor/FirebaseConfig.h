#pragma once
#include <Arduino.h>

class RadiantFirebaseConfig {
public:
  static const char* getFirebaseHost();

  static const char* getDatabaseURL();

  static const char* getApiKey();

  static const char* getProjectId();

  static const char* getAuthEmail();
  static const char* getAuthPassword();

  static const char* getConfigPath();
  static const char* getConfigControlParamsPath();
  static const char* getConfigDhPath();
  static const char* getConfigWeatherKeyPath();
  static const char* getTelemetryBasePath();
  static const char* getStateBasePath();
  static const char* getHeartbeatPath();
  static const char* getDevicesBasePath();
};
