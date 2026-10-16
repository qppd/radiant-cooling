#pragma once
#include <Arduino.h>

struct WeatherConditions {
  bool  ok          = false;
  float tempC       = 0.0f;
  float humidityPct = 0.0f;
  float dewPointC   = 0.0f;
  float feelsLikeC  = 0.0f;
  float windKph     = 0.0f;
  bool  isDay       = true;
  uint32_t obsEpoch = 0;          // WeatherAPI last_updated_epoch
  char  condition[40] = {0};      // e.g. "Partly cloudy"
};

class WeatherApi {
public:
  WeatherApi(const char* location);

  void setKey(const char* key);
  bool hasKey() const;
  const char* key() const { return _key; }

  WeatherConditions fetch();

private:
  char _key[64];
  const char* _location;
};
