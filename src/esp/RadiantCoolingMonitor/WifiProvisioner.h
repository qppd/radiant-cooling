/*
 * WifiProvisioner.h - wifi module
 *
 * Wraps the WiFiManager library (tzapu) for the gateway board:
 *   - connects using saved credentials
 *   - opens a captive portal on first boot / lost network
 *   - a hardware button (hold ~3 s) erases the saved SSID/password so the
 *     device can be re-provisioned
 *
 * Gateway-only - the ESP-NOW peers never join a network.
 */
#pragma once
#include <Arduino.h>
#include <WiFiManager.h>

class WifiProvisioner {
public:
  WifiProvisioner(const char* apName, uint8_t resetPin);

  // Connects with saved credentials, or opens the captive portal
  // (AP "<apName>"). Returns true when joined the network.
  bool begin(unsigned long portalTimeoutS = 180);

  // Call from loop(): erases credentials + restarts when the reset button
  // is held for `holdMs` (default 3000).
  void handleResetButton(unsigned long holdMs = 3000);

  void resetSettings();            // erase saved SSID/password
  void restart();                  // resetSettings + ESP.restart()

private:
  WiFiManager _wm;
  const char* _apName;
  uint8_t _resetPin;
  unsigned long _pressStartMs = 0;
};
