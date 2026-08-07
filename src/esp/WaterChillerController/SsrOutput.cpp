#include "SsrOutput.h"

SsrOutput::SsrOutput(uint8_t pin) : _pin(pin) {}

void SsrOutput::begin() {
  pinMode(_pin, OUTPUT);
  off();
}

void SsrOutput::on()  { set(true); }
void SsrOutput::off() { set(false); }

void SsrOutput::set(bool state) {
  _state = state;
  digitalWrite(_pin, state ? HIGH : LOW);
}

bool SsrOutput::isOn() const {
  return _state;
}
