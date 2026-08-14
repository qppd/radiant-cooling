#include "TemperatureSensor.h"

TemperatureSensor::TemperatureSensor(uint8_t oneWirePin, uint8_t count)
  : _oneWire(oneWirePin), _sensors(&_oneWire), _count(count) {}

bool TemperatureSensor::begin() {
  _sensors.begin();
  return _sensors.getDeviceCount() >= 1;
}

uint8_t TemperatureSensor::count() const {
  return _count;
}

void TemperatureSensor::requestTemperatures() {
  _sensors.requestTemperatures();
}

float TemperatureSensor::readC(uint8_t index) {
  return _sensors.getTempCByIndex(index);
}
