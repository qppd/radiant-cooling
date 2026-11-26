#pragma once
#include <Arduino.h>
#include "PINS_CONFIG.h"
#include "FirebaseConfig.h"

static const char DEVICE_ID[] = "monitor";
static const char SYSTEM_ID[] = "RADIANT-001";

static const char WIFI_AP_NAME[] = "RadiantCooling-AP";
static const uint16_t WIFI_RESET_HOLD_MS = 3000;

static const uint8_t PEER_CHILLER[] = { 0x20, 0x50, 0x0d, 0x1b, 0x9e, 0x48 };
static const uint8_t PEER_DEHUM[]   = { 0x70, 0x4B, 0xCA, 0x17, 0x39, 0xF8 };

static const char   WEATHER_LOCATION[] = "Manila";
static const uint32_t WEATHER_POLL_S   = 900;
static const uint32_t WEATHER_STALE_S  = 3600;

// WeatherAPI key: re-read over REST while we have none (the app may not have
// published it yet) and refresh it occasionally to catch a rotation.
static const uint32_t WEATHER_KEY_RETRY_S   = 60;
static const uint32_t WEATHER_KEY_REFRESH_S = 3600;

// Channel the peers pin their radios to (their ESP_NOW_CHANNEL). The gateway
// follows the router while associated and re-pins to this channel whenever
// the AP is lost, so local control keeps working offline. If the router uses
// a different channel, ESP-NOW sends fail with no ACK and the gateway warns.
static const uint8_t ESP_NOW_CHANNEL = 6;

static const uint8_t IDX_SUPPLY = 0;
static const uint8_t IDX_RETURN = 1;

static const uint8_t TEMP_COUNT  = 4;
static const uint8_t TELEMETRY_S = 5;

// ── Offline-first networking ─────────────────────────────────────────────
// The gateway is a local controller first and a cloud gateway second: losing
// Wi-Fi/Firebase/WeatherAPI must never stop the control loop or reboot the
// board. These timers bound how often the background cloud task retries.
static const uint32_t WIFI_RETRY_MS        = 60000;   // saved-network retry while offline
static const uint32_t CLOUD_RETRY_MS       = 60000;   // first Firebase retry
static const uint32_t CLOUD_RETRY_MAX_MS   = 300000;  // backoff ceiling while internet is down
static const uint32_t PUMP_CMD_RESEND_S    = 60;      // resend current pump command (idempotent)
