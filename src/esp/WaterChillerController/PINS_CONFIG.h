/*
 * PINS_CONFIG.h - pin assignments (WaterChillerController)
 *
 * All GPIO wiring for this board lives here, separated from the rest of
 * the board configuration (Config.h).
 *
 * Pins chosen for the ESP32 38-pin variant (WROOM-32 DevKit / NodeMCU-32S):
 *   - NOT strapping pins   (0, 2, 5, 12, 15  - sampled at boot, affect boot mode)
 *   - NOT flash pins       (6-11             - wired to internal SPI flash, never use)
 *   - NOT UART0            (1, 3             - USB programming / serial monitor)
 *   - NOT ADC2             (0, 2, 4, 12, 13, 14, 15, 25, 26, 27 - unusable with
 *                            analogRead while Wi-Fi / ESP-NOW is active)
 *   -> digital-safe pool: 18, 19, 21, 22, 23, 32, 33
 */
#pragma once
#include <Arduino.h>

// ---- Pin map ----
static const uint8_t PIN_SSR_COMPRESSOR = 18; // SSR -> compressor
static const uint8_t PIN_RELAY_PUMP1    = 21; // 2-channel relay IN1 -> water-in pump
static const uint8_t PIN_RELAY_PUMP2    = 22; // 2-channel relay IN2 -> water-out pump

// Each 3-wire DS18B20 adapter has its own 1-Wire bus (DAT/GND/VCC).
// Confirm whether the adapter already includes the 4.7k pull-up to 3V3.
static const uint8_t PIN_TEMP_OUTGOING = 32; // outgoing water temperature
static const uint8_t PIN_TEMP_INGOING  = 23; // ingoing water temperature
static const uint8_t PIN_TEMP_TANK     = 19; // water-chiller tank temperature
