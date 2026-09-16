#pragma once
#include <Arduino.h>

class SsrOutput {
public:
  explicit SsrOutput(uint8_t pin);

  void begin();
  void on();
  void off();
  void set(bool state);
  bool isOn() const;

private:
  uint8_t _pin;
  bool _state = false;
};
