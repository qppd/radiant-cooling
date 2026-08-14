/*
 * WifiProvisioner.h - wifi module
 *
 * Wraps the WiFiManager library (tzapu) for the gateway board:
 *   - connects using saved credentials
 *   - opens a branded captive portal on first boot / lost network
 *     (fixed AP IP 192.168.4.1, optional AP password, custom HTML page)
 *   - a hardware button (hold ~3 s) erases the saved SSID/password so the
 *     device can be re-provisioned
 *   - reconnectIfLost(): rate-limited auto-reconnect after a drop
 *
 * Gateway-only - the ESP-NOW peers never join a network.
 */
#pragma once
#include <Arduino.h>
#include <WiFiManager.h>

class WifiProvisioner {
public:
  // apPassword: optional password for the portal AP (nullptr = open portal).
  WifiProvisioner(const char* apName, uint8_t resetPin,
                  const char* apPassword = nullptr);

  // Connects with saved credentials, or opens the captive portal
  // (AP "<apName>", fixed IP 192.168.4.1, branded page). Uses the current
  // connect/portal timeouts (set via the setters; defaults 30 s / 180 s).
  bool begin();

  // Custom timeouts in seconds.
  void setConnectTimeout(uint16_t seconds);
  void setConfigPortalTimeout(uint16_t seconds);

  // Call from loop(): erases credentials + restarts when the reset button
  // is held for `holdMs` (default 3000).
  void handleResetButton(unsigned long holdMs = 3000);

  // Call from loop(): retries WiFi.reconnect() (rate-limited to intervalMs)
  // when the connection drops; a no-op while connected.
  void reconnectIfLost(unsigned long intervalMs = 10000);

  bool connected();                   // WiFi.status() == WL_CONNECTED
  String localIP();                   // WiFi.localIP().toString()

  void resetSettings();               // erase saved SSID/password
  void restart();                     // resetSettings + ESP.restart()

private:
  WiFiManager _wm;
  const char* _apName;
  const char* _apPassword;
  uint8_t _resetPin;
  uint16_t _connectTimeoutS = 30;     // seconds (WiFiManager default 30)
  uint16_t _portalTimeoutS = 180;     // seconds (3 min)
  unsigned long _pressStartMs = 0;
  unsigned long _lastReconnectMs = 0;
};
