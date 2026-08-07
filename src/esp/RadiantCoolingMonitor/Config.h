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

// ---- Device identity (see docs/api.md) ----
static const char DEVICE_ID[] = "monitor";

// ---- Wi-Fi ----
static const char WIFI_SSID[] = "your-ssid";
static const char WIFI_PASS[] = "your-password";

// ---- Firebase Realtime Database ----
static const char FIREBASE_URL[]    = "https://<project>.firebaseio.com/";
static const char FIREBASE_SECRET[] = "<database-secret-or-auth-token>";

// ---- ESP-NOW peer MAC addresses (one per controller) ----
static const uint8_t PEER_CHILLER[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t PEER_DEHUM[]   = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// ---- Weather API (WeatherAPI.com) ----
static const char WEATHER_API_KEY[]  = "your-weatherapi-key";
static const char WEATHER_LOCATION[] = "your-city";
static const uint16_t WEATHER_POLL_S = 900;    // weather refresh interval (s)

// ---- Fixed constants ----
static const uint8_t TEMP_COUNT  = 6;      // number of DS18B20 sensors
static const uint8_t TELEMETRY_S = 30;     // Firebase publish interval (s)
static const uint8_t FB_POLL_S   = 5;      // Firebase config poll interval (s)
