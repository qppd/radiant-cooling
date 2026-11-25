#include "WifiProvisioner.h"

WifiProvisioner::WifiProvisioner(const char* apName, uint8_t resetPin,
                                 const char* apPassword)
  : _apName(apName), _apPassword(apPassword), _resetPin(resetPin) {}

bool WifiProvisioner::begin(uint8_t preferredChannel) {
  _preferredChannel = preferredChannel;
  pinMode(_resetPin, INPUT_PULLUP);

  // Holding the button at boot erases the saved network and opens the portal.
  if (digitalRead(_resetPin) == LOW) {
    delay(500);
    if (digitalRead(_resetPin) == LOW) resetSettings();
  }

  _wm.setConfigPortalTimeout(_portalTimeoutS);
  _wm.setConnectTimeout(_connectTimeoutS);
  _wm.setAPStaticIPConfig(
    IPAddress(192, 168, 4, 1),
    IPAddress(192, 168, 4, 1),
    IPAddress(255, 255, 255, 0)
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

  // The core retries a failed association immediately and repeatedly, which
  // means continuous channel scans while the router is missing - that knocks
  // ESP-NOW off the peers' pinned channel. Retry on our own schedule instead.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);

  if (!_wm.getWiFiIsSaved()) {
    Serial.println("[wifi] no saved network - starting setup portal");
    return startPortal();
  }

  String ssid, pass;
  if (!readStored(ssid, pass)) {
    Serial.println("[wifi] saved network unreadable - starting setup portal");
    return startPortal();
  }

  Serial.printf("[wifi] connecting to \"%s\" on channel %u "
                "(local control runs meanwhile)\n",
                ssid.c_str(), (unsigned)(_preferredChannel ? _preferredChannel : 0));
  WiFi.begin(ssid.c_str(), pass.c_str(),
             _preferredChannel ? (int32_t)_preferredChannel : 0);
  _pinnedAttempts = _preferredChannel ? 1 : 0;
  _lastReconnectMs = millis();
  _lastFullScanMs = millis();
  return true;
}

bool WifiProvisioner::startPortal() {
  const bool ok = _wm.autoConnect(_apName, _apPassword);

  // autoConnect may leave AP+STA mode when the portal times out. Go back to a
  // plain station so the caller can pin the ESP-NOW channel and keep running.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);

  if (!ok) {
    Serial.println("[wifi] setup portal timed out - continuing offline");
    WiFi.disconnect(false, false);
    return false;
  }

  Serial.println("[wifi] credentials saved");
  _pinnedAttempts = 0;
  _lastReconnectMs = 0;
  return true;
}

void WifiProvisioner::setConnectTimeout(uint16_t seconds) {
  _connectTimeoutS = seconds;
}

void WifiProvisioner::setConfigPortalTimeout(uint16_t seconds) {
  _portalTimeoutS = seconds;
}

bool WifiProvisioner::resetRequested(unsigned long holdMs) {
  if (digitalRead(_resetPin) != LOW) {
    _pressStartMs = 0;
    return false;
  }
  if (_pressStartMs == 0) _pressStartMs = millis();
  return millis() - _pressStartMs >= holdMs;
}

void WifiProvisioner::reconnectIfLost(unsigned long intervalMs) {
  if (connected()) {
    _lastReconnectMs = 0;
    _pinnedAttempts = 0;
    return;
  }

  const unsigned long now = millis();
  if (_lastReconnectMs != 0 && now - _lastReconnectMs < intervalMs) return;
  _lastReconnectMs = now;

  String ssid, pass;
  if (!readStored(ssid, pass)) return;

  // Stay on the ESP-NOW channel for the first few tries (no disruptive full
  // scan). After that, allow a normal scan occasionally so a router that moved
  // channel can still be reached - the loop will then warn about the ESP-NOW
  // mismatch. Keeping the scan rare (every 5 min) means the radio is almost
  // always on the peers' channel while the router is missing.
  static const unsigned long kFullScanIntervalMs = 300000UL;
  int32_t channel = _preferredChannel ? (int32_t)_preferredChannel : 0;
  if (_preferredChannel && _pinnedAttempts >= 3 &&
      millis() - _lastFullScanMs >= kFullScanIntervalMs) {
    Serial.printf("[wifi] network not found on channel %u - scanning all "
                  "channels (ESP-NOW needs the router on channel %u)\n",
                  (unsigned)_preferredChannel, (unsigned)_preferredChannel);
    channel = 0;
    _lastFullScanMs = millis();
  }
  if (_preferredChannel && _pinnedAttempts < 255) _pinnedAttempts++;

  Serial.printf("[wifi] offline - retrying \"%s\"%s\n", ssid.c_str(),
                channel ? "" : " (full scan)");
  WiFi.disconnect(false, false);
  WiFi.begin(ssid.c_str(), pass.c_str(), channel);
}

bool WifiProvisioner::connected() {
  return WiFi.status() == WL_CONNECTED;
}

String WifiProvisioner::localIP() {
  return WiFi.localIP().toString();
}

bool WifiProvisioner::readStored(String& ssid, String& pass) {
  ssid = _wm.getWiFiSSID(true);
  pass = _wm.getWiFiPass(true);
  ssid.trim();
  return ssid.length() > 0;
}

void WifiProvisioner::resetSettings() {
  _wm.resetSettings();
}

void WifiProvisioner::restart() {
  resetSettings();
  ESP.restart();
}
