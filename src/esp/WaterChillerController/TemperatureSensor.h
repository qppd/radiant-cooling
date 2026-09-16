#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class TemperatureSensor {
public:
  TemperatureSensor(uint8_t oneWirePin, uint8_t count);

  bool begin();
  uint8_t count() const;

  void requestTemperatures();
  float readC(uint8_t index);

private:
  OneWire _oneWire;
  DallasTemperature _sensors;
  uint8_t _count;
};
