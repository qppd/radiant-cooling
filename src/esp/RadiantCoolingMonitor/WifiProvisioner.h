#pragma once
#include <Arduino.h>
#include <WiFiManager.h>

// Offline-first Wi-Fi helper.
//
// The gateway is a local controller first: when credentials are already saved
// the association is started in the background and begin() returns at once.
// Nothing here ever calls ESP.restart() on a network failure - the caller keeps
// running the sensor/ESP-NOW control loop and retries via reconnectIfLost().
class WifiProvisioner {
public:
  WifiProvisioner(const char* apName, uint8_t resetPin,
                  const char* apPassword = nullptr);

  // preferredChannel keeps reconnect attempts on the ESP-NOW channel, so the
  // radio does not scan away from the peers while the router is missing.
  // Blocks only for the configuration portal (no saved credentials).
  bool begin(uint8_t preferredChannel = 0);

  void setConnectTimeout(uint16_t seconds);
  void setConfigPortalTimeout(uint16_t seconds);

  // True once the reset button has been held long enough. The caller should
  // stop actuators, then call restart() (which erases the credentials).
  bool resetRequested(unsigned long holdMs = 3000);

  // Retry the saved network while offline (default once a minute). After a
  // few pinned-channel failures it falls back to a normal scan at most every
  // 5 minutes, so a router that changed channel can still be reached without
  // pulling the radio off the peers' channel most of the time.
  void reconnectIfLost(unsigned long intervalMs = 60000);

  bool connected();
  String localIP();

  void resetSettings();
  void restart();

private:
  bool startPortal();
  bool readStored(String& ssid, String& pass);

  WiFiManager _wm;
  const char* _apName;
  const char* _apPassword;
  uint8_t _resetPin;
  uint16_t _connectTimeoutS = 30;
  uint16_t _portalTimeoutS = 180;
  uint8_t _preferredChannel = 0;
  uint8_t _pinnedAttempts = 0;
  unsigned long _pressStartMs = 0;
  unsigned long _lastReconnectMs = 0;
  unsigned long _lastFullScanMs = 0;
};
