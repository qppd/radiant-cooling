#include "WeatherApi.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

WeatherApi::WeatherApi(const char* location)
  : _location(location) {
  _key[0] = '\0';
}

void WeatherApi::setKey(const char* key) {
  strlcpy(_key, key, sizeof(_key));
}

bool WeatherApi::hasKey() const {
  return _key[0] != '\0';
}

WeatherConditions WeatherApi::fetch() {
  WeatherConditions out;
  if (!hasKey()) return out;

  HTTPClient http;
  String url = String("https://api.weatherapi.com/v1/current.json?key=")
             + _key + "&q=" + _location + "&aqi=no";
  // HTTPS needs the ESP32 core's built-in TLS (cert bundle). If the GET
  // fails on your core version, fall back to WiFiClientSecure + setInsecure().
  if (!http.begin(url)) {
    http.end();
    return out;
  }
  http.setConnectTimeout(5000);
  http.setTimeout(10000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return out;
  }

  JsonDocument doc;
  if (deserializeJson(doc, http.getString()) != DeserializationError::Ok) {
    http.end();
    return out;
  }

  JsonObjectConst cur = doc["current"];
  out.tempC       = cur["temp_c"]     | 0.0f;
  out.humidityPct = cur["humidity"]   | 0.0f;
  out.dewPointC   = cur["dewpoint_c"] | 0.0f;
  out.ok          = true;

  http.end();
  return out;
}
