#include "EspNowTransport.h"

EspNowReceiveCb EspNowTransport::_recvHandler = nullptr;
EspNowSendCb    EspNowTransport::_sendHandler = nullptr;

bool EspNowTransport::begin() {
  WiFi.mode(WIFI_STA);                     // ESP-NOW requires station mode
  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_recv_cb(_recvCb);
  esp_now_register_send_cb(_sendCb);
  return true;
}

bool EspNowTransport::addPeer(const uint8_t* mac, uint8_t channel) {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = channel;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

bool EspNowTransport::sendTo(const uint8_t* mac, const uint8_t* data, size_t len) {
  return esp_now_send(mac, data, len) == ESP_OK;
}

void EspNowTransport::onReceive(EspNowReceiveCb cb) { _recvHandler = cb; }
void EspNowTransport::onSend(EspNowSendCb cb)       { _sendHandler = cb; }

void EspNowTransport::_recvCb(const uint8_t* mac, const uint8_t* data, int len) {
  if (_recvHandler) _recvHandler(mac, data, (size_t)len);
}

void EspNowTransport::_sendCb(const uint8_t* mac, esp_now_send_status_t status) {
  if (_sendHandler) _sendHandler(mac, status == ESP_NOW_SEND_SUCCESS);
}
