/*
 * WeatherApi.h - cloud module
 *
 * Wraps the HTTPClient library (built into the Arduino ESP32 core) for the
 * WeatherAPI.com current-weather endpoint. Gateway-only; provides outdoor
 * temperature, humidity, and dew point for the climate computation.
 *
 * The API key is NOT compiled into the firmware - the Flutter app manages
 * it and delivers it at runtime via `radiant/config/weather_key`
 * (Firebase), which the gateway applies with setKey().
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
  WeatherApi(const char* location);

  // App-managed API key, received from the config/weather_key stream.
  void setKey(const char* key);
  bool hasKey() const;

  // Blocking HTTP GET - call it throttled from loop() (see WEATHER_POLL_S).
  // Returns ok=false when the key is missing or the request failed.
  WeatherConditions fetch();

private:
  char _key[64];
  const char* _location;
};
