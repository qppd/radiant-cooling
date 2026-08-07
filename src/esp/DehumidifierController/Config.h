/*
 * Config.h - board configuration (DehumidifierController)
 *
 * Device identity, gateway MAC, and constants for this board.
 * Pin assignments live in PINS_CONFIG.h (included below) - keep hardware
 * wiring separate from configuration.
 */
#pragma once
#include <Arduino.h>
#include "PINS_CONFIG.h"

// ---- Device identity (see docs/api.md) ----
static const char DEVICE_ID[] = "dh";

// ---- ESP-NOW gateway (RadiantCoolingMonitor) MAC address ----
static const uint8_t GATEWAY_MAC[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// ---- Fixed constants ----
static const uint8_t TELEMETRY_S = 30;     // telemetry send interval (s)
