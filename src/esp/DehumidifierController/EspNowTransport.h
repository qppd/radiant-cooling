#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <stdint.h>

typedef void (*EspNowReceiveCb)(const uint8_t* mac, const uint8_t* data, size_t len);
typedef void (*EspNowSendCb)(const uint8_t* mac, bool success);

class EspNowTransport {
public:
  bool begin();
  bool addPeer(const uint8_t* mac, uint8_t channel = 0);
  bool sendTo(const uint8_t* mac, const uint8_t* data, size_t len);
  void onReceive(EspNowReceiveCb cb);
  void onSend(EspNowSendCb cb);

private:
  static void _recvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len);
  static void _sendCb(const esp_now_send_info_t* txInfo, esp_now_send_status_t status);
  static EspNowReceiveCb _recvHandler;
  static EspNowSendCb _sendHandler;
};
