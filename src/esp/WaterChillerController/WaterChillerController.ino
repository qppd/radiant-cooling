
#include "Config.h"
#include "TemperatureSensor.h"
#include "SsrOutput.h"
#include "EspNowTransport.h"
#include "JsonProtocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

TemperatureSensor outgoingTemp(PIN_TEMP_OUTGOING, 1);
TemperatureSensor ingoingTemp(PIN_TEMP_INGOING, 1);
TemperatureSensor tankTemp(PIN_TEMP_TANK, 1);
SsrOutput compressor(PIN_SSR_COMPRESSOR);
SsrOutput pump1(PIN_RELAY_PUMP1);
SsrOutput pump2(PIN_RELAY_PUMP2);
EspNowTransport espNow;

unsigned long lastSendMs = 0;
uint32_t seq = 0;

typedef struct {
  uint8_t mac[6];
  uint8_t data[250];
  size_t  len;
} EspNowRxPacket;
QueueHandle_t espNowQueue;


void sendMsg(MsgType type, const JsonDocument& payload) {
  char buf[250];
  size_t n = JsonProtocol::encode(type, DEVICE_ID, ++seq, payload, buf, sizeof(buf));
  if (n > 0 && n < sizeof(buf)) espNow.sendTo(GATEWAY_MAC, (const uint8_t*)buf, n);
}

void sendStatus() {
  JsonDocument payload;
  payload["online"]   = true;
  payload["firmware"] = "0.1.0";
  sendMsg(MsgType::Status, payload);
}

void setPumps(bool on) {
  if (compressor.isOn() == on && pump1.isOn() == on && pump2.isOn() == on) return;
  compressor.set(on);
  pump1.set(on);
  pump2.set(on);
  JsonDocument payload;
  payload["compressor"] = on ? "on" : "off";
  payload["pump1"] = on ? "on" : "off";
  payload["pump2"] = on ? "on" : "off";
  sendMsg(MsgType::State, payload);
  Serial.printf("[chiller] pumps %s\n", on ? "ON" : "OFF");
}

void handleMessage(const uint8_t* mac, const uint8_t* data, size_t len) {
  if (len == 0 || len > 250 || !espNowQueue) return;
  EspNowRxPacket pkt;
  memcpy(pkt.mac, mac, 6);
  memcpy(pkt.data, data, len);
  pkt.len = len;
  if (xQueueSend(espNowQueue, &pkt, 0) != pdTRUE) {
    Serial.println("[espnow] RX queue full - packet dropped");
  }
}

void handleSendResult(const uint8_t* mac, bool success) {
  Serial.printf("[espnow] send %s -> %02X:%02X:%02X:%02X:%02X:%02X\n",
                success ? "OK" : "FAIL",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void handleCmd(const IncomingMessage& msg) {
  const char* cmd = msg.data["cmd"] | "";
  if (strcmp(cmd, "set_pumps") == 0) {
    setPumps(String(msg.data["value"] | "") == "on");
  }
  else {
    Serial.printf("[chiller] unknown cmd: %s\n", cmd);
  }
}

void setup() {
  Serial.begin(115200);

  espNowQueue = xQueueCreate(8, sizeof(EspNowRxPacket));

  outgoingTemp.begin();
  ingoingTemp.begin();
  tankTemp.begin();
  compressor.begin();
  pump1.begin();
  pump2.begin();

  if (!espNow.begin()) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  espNow.addPeer(GATEWAY_MAC);
  espNow.onReceive(handleMessage);
  espNow.onSend(handleSendResult);

  sendStatus();
}

void loop() {
  outgoingTemp.requestTemperatures();
  ingoingTemp.requestTemperatures();
  tankTemp.requestTemperatures();
  delay(750);
  float outgoingC = outgoingTemp.readC(0);
  float ingoingC  = ingoingTemp.readC(0);
  float tankC     = tankTemp.readC(0);

  if (outgoingC <= -100.0f || ingoingC <= -100.0f || tankC <= -100.0f) {
    setPumps(false);
  }

  EspNowRxPacket pkt;
  while (espNowQueue && xQueueReceive(espNowQueue, &pkt, 0) == pdTRUE) {
    IncomingMessage msg;
    if (JsonProtocol::decode((const char*)pkt.data, pkt.len, msg)) {
      if (msg.type == MsgType::Cmd) handleCmd(msg);
    }
  }

  if (millis() - lastSendMs >= TELEMETRY_S * 1000UL) {
    lastSendMs = millis();
    JsonDocument payload;
    payload["outgoing_temp_c"] = outgoingC;
    payload["ingoing_temp_c"] = ingoingC;
    payload["tank_temp_c"] = tankC;
    payload["water_temp_c"] = tankC;
    payload["compressor"] = compressor.isOn() ? "on" : "off";
    payload["pump1"] = pump1.isOn() ? "on" : "off";
    payload["pump2"] = pump2.isOn() ? "on" : "off";
    sendMsg(MsgType::Telemetry, payload);
  }

  delay(100);
}
