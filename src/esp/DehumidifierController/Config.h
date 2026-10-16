#pragma once
#include <Arduino.h>
#include "PINS_CONFIG.h"

static const char DEVICE_ID[] = "dh";

// Gateway = RadiantCoolingMonitor (20:50:0D:1B:9E:F4)
// Scanned live via `esptool read_mac` on COM3 + `wifi` console cmd.
static const uint8_t GATEWAY_MAC[] = { 0x20, 0x50, 0x0D, 0x1B, 0x9E, 0xF4 };
static const uint8_t TELEMETRY_S = 30;

// ESP-NOW channel this peer pins its radio to. It never joins an AP, so
// without this it idles on channel 1 while the gateway follows the router -
// different channels mean no ACK and every send reports FAIL. Set it to the
// channel the gateway is connected to (the gateway prints it in `wifi`).
static const uint8_t ESP_NOW_CHANNEL = 1;
