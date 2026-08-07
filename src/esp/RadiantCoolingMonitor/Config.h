/*
 * Config.h - board configuration (RadiantCoolingMonitor / gateway)
 *
 * Device identity, credentials, peer MACs, and constants for this board.
 * Pin assignments live in PINS_CONFIG.h (included below) - keep hardware
 * wiring separate from configuration.
 */
#pragma once
#include <Arduino.h>
#include "PINS_CONFIG.h"
#include "WEATHER_CONFIG.h"     // WeatherAPI credentials (git-ignored)
#include "FIREBASE_CONFIG.h"    // Firebase credentials (git-ignored)

// ---- Device identity (see docs/api.md) ----
static const char DEVICE_ID[] = "monitor";

// ---- Wi-Fi (WiFiManager captive portal - credentials are NOT hardcoded) ----
static const char WIFI_AP_NAME[] = "RadiantCooling-AP";  // portal AP name
static const uint16_t WIFI_RESET_HOLD_MS = 3000;         // hold reset button (ms)

// ---- ESP-NOW peer MAC addresses (one per controller) ----
static const uint8_t PEER_CHILLER[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t PEER_DEHUM[]   = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// ---- Weather API (credentials live in WEATHER_CONFIG.h) ----
static const uint16_t WEATHER_POLL_S = 900;    // weather refresh interval (s)

// ---- Fixed constants ----
static const uint8_t TEMP_COUNT  = 6;      // number of DS18B20 sensors
static const uint8_t TELEMETRY_S = 30;     // Firebase publish interval (s)
