#include "WifiProvisioner.h"

WifiProvisioner::WifiProvisioner(const char* apName, uint8_t resetPin)
  : _apName(apName), _resetPin(resetPin) {}

bool WifiProvisioner::begin(unsigned long portalTimeoutS) {
  pinMode(_resetPin, INPUT_PULLUP);

  // Boot-time reset: holding the button while powering up erases credentials
  // so the captive portal re-opens for new SSID/password.
  if (digitalRead(_resetPin) == LOW) {
    delay(500);                              // confirm the hold is intentional
    if (digitalRead(_resetPin) == LOW) resetSettings();
  }

  _wm.setConfigPortalTimeout(portalTimeoutS);
  _wm.setConnectTimeout(15);
  // _wm.setDebugOutput(false);              // optional: quiet serial output

  // autoConnect joins the saved network or starts the portal AP.
  return _wm.autoConnect(_apName);
}

void WifiProvisioner::handleResetButton(unsigned long holdMs) {
  if (digitalRead(_resetPin) == LOW) {
    if (_pressStartMs == 0) _pressStartMs = millis();
    if (millis() - _pressStartMs >= holdMs) restart();
  } else {
    _pressStartMs = 0;                       // released before hold - cancel
  }
}

void WifiProvisioner::resetSettings() {
  _wm.resetSettings();                       // erase saved SSID/password
}

void WifiProvisioner::restart() {
  resetSettings();
  ESP.restart();
}
