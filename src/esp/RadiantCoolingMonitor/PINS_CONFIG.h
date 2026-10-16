#pragma once
#include <Arduino.h>

// 4x DS18B20 ceiling sensors - one dedicated 1-Wire bus per sensor
// (mounted directly on the pipe with a 3D-printed thread mount for a
//  1/2 inch PVC T-connector; each bus needs its own 4.7k pull-up to 3V3)
static const uint8_t PIN_TEMP_CEILING_1 = 23;
static const uint8_t PIN_TEMP_CEILING_2 = 21;
static const uint8_t PIN_TEMP_CEILING_3 = 18;
static const uint8_t PIN_TEMP_CEILING_4 = 5;

static const uint8_t PIN_WIFI_RESET_BUTTON = 25;
