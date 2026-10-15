#pragma once
#include <Arduino.h>
#include "PINS_CONFIG.h"

static const char DEVICE_ID[] = "dh";

// Gateway = RadiantCoolingMonitor (1C:69:20:A3:04:78)
static const uint8_t GATEWAY_MAC[] = { 0x1C, 0x69, 0x20, 0xA3, 0x04, 0x78 };
static const uint8_t TELEMETRY_S = 30;
