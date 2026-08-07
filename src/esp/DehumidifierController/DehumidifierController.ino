/*
 * DehumidifierController.ino
 *
 * ESP32 ESP-NOW PEER - reads temperature + humidity (DHT22) and switches
 * the dehumidifier (SSR) to hold relative humidity at the 55% setpoint
 * (with hysteresis). Sends telemetry to the gateway (RadiantCoolingMonitor)
 * so the humidity reading can be used in the chiller control computation.
 *
 * This file is glue only. All component/library code is encapsulated:
 *   Config.h            - board configuration (MACs, constants); includes PINS_CONFIG.h
 *   PINS_CONFIG.h       - pin assignments (SSR, DHT22)
 *   HumiditySensor      - wraps the DHT library (1x DHT22)
 *   SsrOutput           - wraps one SSR digital output (dehumidifier)
 *   EspNowTransport     - wraps WiFi + esp_now (register gateway, send/receive)
 *   JsonProtocol        - wraps ArduinoJson (ESP-NOW message envelope)
 *
 * See docs/ for architecture, API and control logic.
 */

#include "Config.h"
#include "HumiditySensor.h"
#include "SsrOutput.h"
#include "EspNowTransport.h"
#include "JsonProtocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ---- Components ----
HumiditySensor roomClimate(PIN_DHT22);
SsrOutput dehumidifier(PIN_SSR_DEHUM);
EspNowTransport espNow;

// ---- Runtime config (defaults; updated via ESP-NOW config messages) ----
float HUMIDITY_SETPOINT_PCT = 55.0;   // target relative humidity (%)
float HUMIDITY_DEADBAND_PCT = 5.0;    // hysteresis (%)
bool  systemEnabled         = true;   // cmd enable/disable

// ---- Telemetry state ----
unsigned long lastSendMs = 0;
uint32_t seq = 0;
float lastTempC       = -127.0f;      // last good reading (for telemetry)
float lastHumidityPct = 0.0f;
uint8_t readFailures  = 0;            // consecutive sensor read failures
bool  failSafeTripped = false;        // dehumidifier force-off active (logged once)

// ESP-NOW receive -> processing is decoupled via a FreeRTOS queue: the
// receive callback ONLY enqueues raw bytes (never blocks / decodes inside
// the callback); loop() drains and processes.
typedef struct {
  uint8_t mac[6];
  uint8_t data[250];         // ESP-NOW max payload
  size_t  len;
} EspNowRxPacket;
QueueHandle_t espNowQueue;

// ---- Helpers ----

// Encode + send a JSON message to the gateway.
void sendMsg(MsgType type, const JsonDocument& payload) {
  char buf[250];
  size_t n = JsonProtocol::encode(type, DEVICE_ID, ++seq, payload, buf, sizeof(buf));
  // n == maxLen means the JSON was truncated (serializeJson caps at maxLen).
  if (n > 0 && n < sizeof(buf)) espNow.sendTo(GATEWAY_MAC, (const uint8_t*)buf, n);
}

// Announce online to the gateway on boot.
void sendStatus() {
  JsonDocument payload;
  payload["online"]   = true;
  payload["firmware"] = "0.1.0";
  sendMsg(MsgType::Status, payload);
}

// ---- ESP-NOW receive callback: enqueue only, do not block ----
void handleMessage(const uint8_t* mac, const uint8_t* data, size_t len) {
  if (len == 0 || len > 250 || !espNowQueue) return;
  EspNowRxPacket pkt;
  memcpy(pkt.mac, mac, 6);
  memcpy(pkt.data, data, len);
  pkt.len = len;
  // esp_now callbacks run in the espnow TASK context, so xQueueSend is correct.
  if (xQueueSend(espNowQueue, &pkt, 0) != pdTRUE) {
    Serial.println("[espnow] RX queue full - packet dropped");
  }
}

// ---- ESP-NOW delivery status ----
void handleSendResult(const uint8_t* mac, bool success) {
  Serial.printf("[espnow] send %s -> %02X:%02X:%02X:%02X:%02X:%02X\n",
                success ? "OK" : "FAIL",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ---- Apply a decoded config message from the gateway ----
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

// ---- Apply a decoded command from the gateway ----
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
  // Unknown commands are ignored and logged.
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
  espNow.onReceive(handleMessage);      // enqueue only - processed in loop()
  espNow.onSend(handleSendResult);      // delivery status

  sendStatus();                         // announce online to the gateway
}

void loop() {
  float tempC, humidityPct;

  // Humidity control with hysteresis (see docs/diagrams/flow-chart.md)
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
      // Between the two thresholds: keep the current state (hysteresis)
    }
  } else {
    // Fail-safe: after several consecutive read failures, stop the
    // dehumidifier rather than leave it running blindly, and tell the
    // gateway the new state (logged + sent once per trip).
    if (++readFailures >= 3 && !failSafeTripped) {
      failSafeTripped = true;
      dehumidifier.off();
      JsonDocument payload;
      payload["dehumidifier"] = "off";
      sendMsg(MsgType::State, payload);
      Serial.println("[dh] sensor read failed x3 - dehumidifier off (fail-safe)");
    }
  }

  // Drain the ESP-NOW RX queue (config + commands from the gateway)
  EspNowRxPacket pkt;
  while (espNowQueue && xQueueReceive(espNowQueue, &pkt, 0) == pdTRUE) {
    IncomingMessage msg;
    if (JsonProtocol::decode((const char*)pkt.data, pkt.len, msg)) {
      if (msg.type == MsgType::Config) handleConfig(msg);
      else if (msg.type == MsgType::Cmd) handleCmd(msg);
    }
  }

  // Periodic telemetry to gateway (see docs/api.md)
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
