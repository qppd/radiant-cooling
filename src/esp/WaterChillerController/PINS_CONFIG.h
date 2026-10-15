#pragma once
#include <Arduino.h>

// 1x compressor SSR + 1x pump relay (single pump loop)
static const uint8_t PIN_SSR_COMPRESSOR = 23;
static const uint8_t PIN_RELAY_PUMP1    = 19;

// 2x DS18B20 tank sensors - one dedicated 1-Wire bus each
// (4.7k pull-up to 3V3 per bus)
// outgoing = water leaving the tank, ingoing = water returning to the tank
// NOTE: GPIO 5 is an ESP32 strapping pin - the 4.7k pull-up holds it HIGH at
// boot, which is its safe/default state, so no boot issue with the sensor on it.
static const uint8_t PIN_TEMP_OUTGOING = 5;
static const uint8_t PIN_TEMP_INGOING  = 18;
