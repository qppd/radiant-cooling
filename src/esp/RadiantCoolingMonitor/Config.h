#pragma once
#include <Arduino.h>
#include "PINS_CONFIG.h"
#include "FirebaseConfig.h"

static const char DEVICE_ID[] = "monitor";
static const char SYSTEM_ID[] = "RADIANT-001";

static const char WIFI_AP_NAME[] = "RadiantCooling-AP";
static const uint16_t WIFI_RESET_HOLD_MS = 3000;

static const uint8_t PEER_CHILLER[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t PEER_DEHUM[]   = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static const char   WEATHER_LOCATION[] = "Manila";
static const uint32_t WEATHER_POLL_S   = 900;
static const uint32_t WEATHER_STALE_S  = 3600;

static const uint8_t IDX_SUPPLY = 0;
static const uint8_t IDX_RETURN = 1;

static const uint8_t TEMP_COUNT  = 6;
static const uint8_t TELEMETRY_S = 30;
