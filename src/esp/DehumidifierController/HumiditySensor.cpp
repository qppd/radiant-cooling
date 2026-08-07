#include "HumiditySensor.h"

HumiditySensor::HumiditySensor(uint8_t pin, uint8_t type) : _dht(pin, type) {}

void HumiditySensor::begin() {
  _dht.begin();
}

bool HumiditySensor::read(float& tempC, float& humidityPct) {
  tempC       = _dht.readTemperature();
  humidityPct = _dht.readHumidity();
  return !isnan(tempC) && !isnan(humidityPct);
}
