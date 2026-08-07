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
static const uint8_t PIN_SSR_PUMP1 = 19;   // SSR -> water pump 1
static const uint8_t PIN_SSR_PUMP2 = 21;   // SSR -> water pump 2
static const uint8_t PIN_ONE_WIRE  = 22;   // 1-Wire bus (1x DS18B20) + 4.7k pull-up to 3V3
