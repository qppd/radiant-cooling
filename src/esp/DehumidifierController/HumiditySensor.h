#pragma once
#include <Arduino.h>
#include <DHT.h>

class HumiditySensor {
public:
  explicit HumiditySensor(uint8_t pin, uint8_t type = DHT22);

  void begin();

  bool read(float& tempC, float& humidityPct);

private:
  DHT _dht;
};
