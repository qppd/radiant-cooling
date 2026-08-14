/*
 * TemperatureSensor.h - component module
 *
 * Wraps the OneWire + DallasTemperature libraries for DS18B20 sensors.
 * All DS18B20 devices share one 1-Wire bus; they are addressed by index
 * (0 .. count-1) after begin().
 */
#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class TemperatureSensor {
public:
  TemperatureSensor(uint8_t oneWirePin, uint8_t count);

  bool begin();                       // true if >= 1 sensor found on the bus
  uint8_t count() const;              // expected number of sensors

  void requestTemperatures();         // start conversion (async, ~750 ms)
  float readC(uint8_t index);         // -127.0 when the sensor is disconnected

private:
  OneWire _oneWire;
  DallasTemperature _sensors;
  uint8_t _count;
};
