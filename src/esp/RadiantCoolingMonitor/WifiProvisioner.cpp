#include "WifiProvisioner.h"

WifiProvisioner::WifiProvisioner(const char* apName, uint8_t resetPin,
                                 const char* apPassword)
  : _apName(apName), _apPassword(apPassword), _resetPin(resetPin) {}

bool WifiProvisioner::begin() {
  pinMode(_resetPin, INPUT_PULLUP);

  // Boot-time reset: holding the button while powering up erases credentials
  // so the captive portal re-opens for new SSID/password.
  if (digitalRead(_resetPin) == LOW) {
    delay(500);                              // confirm the hold is intentional
    if (digitalRead(_resetPin) == LOW) resetSettings();
  }

  _wm.setConfigPortalTimeout(_portalTimeoutS);
  _wm.setConnectTimeout(_connectTimeoutS);
  // _wm.setDebugOutput(false);              // optional: quiet serial output

  // Captive portal: fixed AP IP (192.168.4.1) + branded page.
  _wm.setAPStaticIPConfig(
    IPAddress(192, 168, 4, 1),   // AP IP
    IPAddress(192, 168, 4, 1),   // gateway
    IPAddress(255, 255, 255, 0)  // netmask
  );
  _wm.setCustomHeadElement(R"(
<style>
  body { background: linear-gradient(135deg, #0e7490 0%, #155e75 100%);
         font-family: Arial, sans-serif; }
  .container { background: #ffffff; border-radius: 12px; padding: 24px;
               max-width: 400px; margin: 48px auto;
               box-shadow: 0 4px 12px rgba(0,0,0,0.25); }
  h1 { color: #155e75; text-align: center; margin-bottom: 6px; }
  p  { color: #64748b; text-align: center; font-size: 14px; }
  input, button { width: 100%; padding: 12px; margin: 10px 0;
                  border-radius: 6px; border: 1px solid #cbd5e1;
                  box-sizing: border-box; }
  input { font-size: 16px; }
  button { background: #0e7490; color: #fff; border: none; cursor: pointer;
           font-weight: bold; }
  button:hover { background: #155e75; }
</style>
)");

  // autoConnect joins the saved network or starts the (branded) portal AP.
  return _wm.autoConnect(_apName, _apPassword);
}

void WifiProvisioner::setConnectTimeout(uint16_t seconds) {
  _connectTimeoutS = seconds;
}

void WifiProvisioner::setConfigPortalTimeout(uint16_t seconds) {
  _portalTimeoutS = seconds;
}

void WifiProvisioner::handleResetButton(unsigned long holdMs) {
  if (digitalRead(_resetPin) == LOW) {
    if (_pressStartMs == 0) _pressStartMs = millis();
    if (millis() - _pressStartMs >= holdMs) restart();
  } else {
    _pressStartMs = 0;                       // released before hold - cancel
  }
}

void WifiProvisioner::reconnectIfLost(unsigned long intervalMs) {
  if (connected()) {
    _lastReconnectMs = 0;                    // fresh drop -> retry immediately
    return;
  }
  const unsigned long now = millis();
  if (_lastReconnectMs == 0 || now - _lastReconnectMs >= intervalMs) {
    _lastReconnectMs = now;
    Serial.println("[wifi] connection lost - reconnecting...");
    WiFi.disconnect(false);                  // keep the radio on
    WiFi.reconnect();                        // retry with saved credentials
  }
}

bool WifiProvisioner::connected() {
  return WiFi.status() == WL_CONNECTED;
}

String WifiProvisioner::localIP() {
  return WiFi.localIP().toString();
}

void WifiProvisioner::resetSettings() {
  _wm.resetSettings();                       // erase saved SSID/password
}

void WifiProvisioner::restart() {
  resetSettings();
  ESP.restart();
}
