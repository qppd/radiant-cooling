#pragma once
#include <Arduino.h>

struct WeatherConditions {
  bool  ok          = false;
  float tempC       = 0.0f;
  float humidityPct = 0.0f;
  float dewPointC   = 0.0f;
};

class WeatherApi {
public:
  WeatherApi(const char* location);

  void setKey(const char* key);
  bool hasKey() const;

  WeatherConditions fetch();

private:
  char _key[64];
  const char* _location;
};
