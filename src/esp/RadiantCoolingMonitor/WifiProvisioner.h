#pragma once
#include <Arduino.h>
#include <WiFiManager.h>

class WifiProvisioner {
public:
  WifiProvisioner(const char* apName, uint8_t resetPin,
                  const char* apPassword = nullptr);

  bool begin();

  void setConnectTimeout(uint16_t seconds);
  void setConfigPortalTimeout(uint16_t seconds);

  void handleResetButton(unsigned long holdMs = 3000);

  void reconnectIfLost(unsigned long intervalMs = 10000);

  bool connected();
  String localIP();

  void resetSettings();
  void restart();

private:
  WiFiManager _wm;
  const char* _apName;
  const char* _apPassword;
  uint8_t _resetPin;
  uint16_t _connectTimeoutS = 30;
  uint16_t _portalTimeoutS = 180;
  unsigned long _pressStartMs = 0;
  unsigned long _lastReconnectMs = 0;
};
