/*
 * HumiditySensor.h - component module
 *
 * Wraps the DHT library for a DHT22 (temperature + humidity) sensor.
 * Used by the dehumidifier board to compute dew point.
 */
#pragma once
#include <Arduino.h>
#include <DHT.h>

class HumiditySensor {
public:
  explicit HumiditySensor(uint8_t pin, uint8_t type = DHT22);

  void begin();

  // Reads both values; returns false on read failure (sensor missing/NAN).
  bool read(float& tempC, float& humidityPct);

private:
  DHT _dht;
};
