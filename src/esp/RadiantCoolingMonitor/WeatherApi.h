/*
 * WeatherApi.h - cloud module
 *
 * Wraps the HTTPClient library (built into the Arduino ESP32 core) for the
 * WeatherAPI.com current-weather endpoint. Gateway-only; provides outdoor
 * temperature, humidity, and dew point for the climate computation.
 *
 *   GET https://api.weatherapi.com/v1/current.json?key=<KEY>&q=<LOCATION>
 *   -> current.{ temp_c, humidity, dewpoint_c }
 */
#pragma once
#include <Arduino.h>

struct WeatherConditions {
  bool  ok          = false;
  float tempC       = 0.0f;   // outdoor temperature (C)
  float humidityPct = 0.0f;   // outdoor relative humidity (%)
  float dewPointC   = 0.0f;   // outdoor dew point (C)
};

class WeatherApi {
public:
  WeatherApi(const char* apiKey, const char* location);

  // Blocking HTTP GET - call it throttled from loop() (see WEATHER_POLL_S).
  WeatherConditions fetch();

private:
  const char* _apiKey;
  const char* _location;
};
