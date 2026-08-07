/*
 * PINS_CONFIG.h - pin assignments (DehumidifierController)
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
static const uint8_t PIN_SSR_DEHUM = 23;   // SSR -> dehumidifier
static const uint8_t PIN_DHT22     = 32;   // DHT22 data pin (add 10k pull-up to 3V3)
