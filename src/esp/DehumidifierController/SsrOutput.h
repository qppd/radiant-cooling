/*
 * SsrOutput.h - component module
 *
 * Wraps a solid state relay (SSR) digital output. Used to switch water
 * pumps (chiller board) and the dehumidifier (dehumidifier board).
 */
#pragma once
#include <Arduino.h>

class SsrOutput {
public:
  explicit SsrOutput(uint8_t pin);

  void begin();            // pin as output, starts OFF
  void on();
  void off();
  void set(bool state);
  bool isOn() const;

private:
  uint8_t _pin;
  bool _state = false;
};
