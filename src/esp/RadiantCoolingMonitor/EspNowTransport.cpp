#include "EspNowTransport.h"
#include <esp_wifi.h>

EspNowReceiveCb EspNowTransport::_recvHandler = nullptr;
EspNowSendCb    EspNowTransport::_sendHandler = nullptr;

bool EspNowTransport::begin(uint8_t offlineChannel) {
  WiFi.mode(WIFI_STA);
  // Keep the radio fully awake: with WiFi power-save on (default when
  // associated to an AP), the STA sleeps between DTIM beacons and misses the
  // MAC-layer ACK window - ESP-NOW data is delivered but every send reports
  // FAIL. This must run on the gateway; harmless on the peers.
  WiFi.setSleep(false);
  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_recv_cb(_recvCb);
  esp_now_register_send_cb(_sendCb);

  // While the gateway has no AP it must sit on the same channel the peers
  // pinned themselves to, otherwise every ESP-NOW send fails silently.
  if (offlineChannel != 0 && WiFi.status() != WL_CONNECTED) {
    if (!setChannel(offlineChannel)) {
      Serial.printf("[espnow] could not pin offline channel %u\n",
                    (unsigned)offlineChannel);
    } else {
      Serial.printf("[espnow] offline - radio pinned to channel %u\n",
                    (unsigned)offlineChannel);
    }
  }
  return true;
}

bool EspNowTransport::setChannel(uint8_t channel) {
  if (channel == 0) return false;
  if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.printf("[espnow] could not set channel %u\n", (unsigned)channel);
    return false;
  }
  return true;
}

uint8_t EspNowTransport::channel() {
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK) return 0;
  return primary;
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

void EspNowTransport::_recvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (_recvHandler && info) _recvHandler(info->src_addr, data, (size_t)len);
}

void EspNowTransport::_sendCb(const esp_now_send_info_t* txInfo, esp_now_send_status_t status) {
  if (_sendHandler && txInfo) _sendHandler(txInfo->des_addr, status == ESP_NOW_SEND_SUCCESS);
}
