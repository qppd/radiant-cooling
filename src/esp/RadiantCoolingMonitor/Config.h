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
#include "FIREBASE_CONFIG.h"    // Firebase credentials (git-ignored)

// ---- Device identity (see docs/api.md) ----
static const char DEVICE_ID[] = "monitor";     // ESP-NOW sender id (per board)
static const char SYSTEM_ID[] = "RADIANT-001"; // user-facing system id - links the Flutter app to this installation

// ---- Wi-Fi (WiFiManager captive portal - credentials are NOT hardcoded) ----
static const char WIFI_AP_NAME[] = "RadiantCooling-AP";  // portal AP name
static const uint16_t WIFI_RESET_HOLD_MS = 3000;         // hold reset button (ms)

// ---- ESP-NOW peer MAC addresses (one per controller) ----
static const uint8_t PEER_CHILLER[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t PEER_DEHUM[]   = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// ---- Weather (gateway fetches WeatherAPI itself; key managed by the app) ----
static const char   WEATHER_LOCATION[] = "your-city";  // WeatherAPI q= location
static const uint32_t WEATHER_POLL_S   = 900;           // weather refresh interval (s)
static const uint32_t WEATHER_STALE_S  = 3600;          // outdoor weather older than this is ignored (s)

// ---- DS18B20 roles on the monitor's 1-Wire bus (indices 0..TEMP_COUNT-1) ----
// All sensors are mounted on the chilled-water pipes above the ceiling:
//   0 = water supply, 1 = water return, 2..5 = pipe temperatures along the run
static const uint8_t IDX_SUPPLY = 0;
static const uint8_t IDX_RETURN = 1;

// ---- Fixed constants ----
static const uint8_t TEMP_COUNT  = 6;      // number of DS18B20 sensors
static const uint8_t TELEMETRY_S = 30;     // Firebase publish interval (s)
