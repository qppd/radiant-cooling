
#include "Config.h"
#include "HumiditySensor.h"
#include "SsrOutput.h"
#include "EspNowTransport.h"
#include "JsonProtocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

HumiditySensor roomClimate(PIN_DHT22);
SsrOutput dehumidifier(PIN_SSR_DEHUM);
EspNowTransport espNow;

float HUMIDITY_SETPOINT_PCT = 55.0;
float HUMIDITY_DEADBAND_PCT = 5.0;
bool  systemEnabled         = true;

unsigned long lastSendMs = 0;
uint32_t seq = 0;
float lastTempC       = -127.0f;
float lastHumidityPct = 0.0f;
uint8_t readFailures  = 0;
bool  failSafeTripped = false;

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

void handleConfig(const IncomingMessage& msg) {
  if (msg.data["humidity_setpoint_pct"].is<float>()) {
    HUMIDITY_SETPOINT_PCT = msg.data["humidity_setpoint_pct"];
    Serial.printf("[dh] setpoint -> %.1f %%RH\n", HUMIDITY_SETPOINT_PCT);
  }
  if (msg.data["humidity_deadband_pct"].is<float>()) {
    HUMIDITY_DEADBAND_PCT = msg.data["humidity_deadband_pct"];
    Serial.printf("[dh] deadband -> %.1f %%RH\n", HUMIDITY_DEADBAND_PCT);
  }
}

void handleCmd(const IncomingMessage& msg) {
  const char* cmd = msg.data["cmd"] | "";
  if (strcmp(cmd, "set_humidity_target") == 0) {
    HUMIDITY_SETPOINT_PCT = msg.data["value"] | HUMIDITY_SETPOINT_PCT;
    Serial.printf("[dh] set_humidity_target -> %.1f %%RH\n", HUMIDITY_SETPOINT_PCT);
  } else if (strcmp(cmd, "enable") == 0) {
    systemEnabled = true;
    Serial.println("[dh] enabled");
  } else if (strcmp(cmd, "disable") == 0) {
    systemEnabled = false;
    dehumidifier.off();
    Serial.println("[dh] disabled");
  } else if (strcmp(cmd, "reset") == 0) {
    Serial.println("[dh] reset");
    ESP.restart();
  }
  else {
    Serial.printf("[dh] unknown cmd: %s\n", cmd);
  }
}

void setup() {
  Serial.begin(115200);

  espNowQueue = xQueueCreate(8, sizeof(EspNowRxPacket));

  roomClimate.begin();
  dehumidifier.begin();

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
  float tempC, humidityPct;

  if (roomClimate.read(tempC, humidityPct)) {
    readFailures  = 0;
    failSafeTripped = false;
    lastTempC       = tempC;
    lastHumidityPct = humidityPct;
    if (systemEnabled) {
      if (humidityPct > HUMIDITY_SETPOINT_PCT + HUMIDITY_DEADBAND_PCT) {
        dehumidifier.on();
      } else if (humidityPct < HUMIDITY_SETPOINT_PCT - HUMIDITY_DEADBAND_PCT) {
        dehumidifier.off();
      }
    }
  } else {
    if (++readFailures >= 3 && !failSafeTripped) {
      failSafeTripped = true;
      dehumidifier.off();
      JsonDocument payload;
      payload["dehumidifier"] = "off";
      sendMsg(MsgType::State, payload);
      Serial.println("[dh] sensor read failed x3 - dehumidifier off (fail-safe)");
    }
  }

  EspNowRxPacket pkt;
  while (espNowQueue && xQueueReceive(espNowQueue, &pkt, 0) == pdTRUE) {
    IncomingMessage msg;
    if (JsonProtocol::decode((const char*)pkt.data, pkt.len, msg)) {
      if (msg.type == MsgType::Config) handleConfig(msg);
      else if (msg.type == MsgType::Cmd) handleCmd(msg);
    }
  }

  if (millis() - lastSendMs >= TELEMETRY_S * 1000UL) {
    lastSendMs = millis();
    JsonDocument payload;
    payload["temp_c"]        = lastTempC;
    payload["humidity_pct"]  = lastHumidityPct;
    payload["dehumidifier"]  = dehumidifier.isOn() ? "on" : "off";
    sendMsg(MsgType::Telemetry, payload);
  }

  delay(100);
}
