/*
 * WaterChillerController.ino
 *
 * ESP32 ESP-NOW PEER - reads water temperature (1x DS18B20) and switches
 * the two water pumps (SSR) on COMMAND from the gateway.
 *
 * The pump on/off decision is computed on the gateway (RadiantCoolingMonitor)
 * from the WeatherAPI dew point + all sensor readings (see
 * docs/diagrams/flow-chart.md). This board executes commands and reports
 * water temperature back; it only overrides for local fail-safety.
 *
 * This file is glue only. All component/library code is encapsulated:
 *   Config.h            - board configuration (MACs, constants); includes PINS_CONFIG.h
 *   PINS_CONFIG.h       - pin assignments (SSRs, 1-Wire)
 *   TemperatureSensor   - wraps OneWire + DallasTemperature (1x DS18B20)
 *   SsrOutput           - wraps one SSR digital output (pump 1, pump 2)
 *   EspNowTransport     - wraps WiFi + esp_now (register gateway, send/receive)
 *   JsonProtocol        - wraps ArduinoJson (ESP-NOW message envelope)
 *
 * See docs/ for architecture, API and control logic.
 */

#include "Config.h"
#include "TemperatureSensor.h"
#include "SsrOutput.h"
#include "EspNowTransport.h"
#include "JsonProtocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ---- Components ----
TemperatureSensor waterTemp(PIN_ONE_WIRE, TEMP_COUNT);
SsrOutput pump1(PIN_SSR_PUMP1);
SsrOutput pump2(PIN_SSR_PUMP2);
EspNowTransport espNow;

// ---- Telemetry state ----
unsigned long lastSendMs = 0;
uint32_t seq = 0;

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

// Execute the gateway's pump command.
void setPumps(bool on) {
  if (pump1.isOn() == on && pump2.isOn() == on) return;   // no change
  pump1.set(on);
  pump2.set(on);
  JsonDocument payload;
  payload["pump1"] = on ? "on" : "off";
  payload["pump2"] = on ? "on" : "off";
  sendMsg(MsgType::State, payload);
  Serial.printf("[chiller] pumps %s\n", on ? "ON" : "OFF");
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

// ---- Apply a decoded command from the gateway ----
void handleCmd(const IncomingMessage& msg) {
  const char* cmd = msg.data["cmd"] | "";
  if (strcmp(cmd, "set_pumps") == 0) {
    setPumps(String(msg.data["value"] | "") == "on");
  }
  // Unknown commands are ignored and logged.
  else {
    Serial.printf("[chiller] unknown cmd: %s\n", cmd);
  }
}

void setup() {
  Serial.begin(115200);

  espNowQueue = xQueueCreate(8, sizeof(EspNowRxPacket));

  waterTemp.begin();
  pump1.begin();
  pump2.begin();

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
  waterTemp.requestTemperatures();
  delay(750);                                // DS18B20 conversion time
  float tempC = waterTemp.readC(0);

  // Local fail-safe only: if the water sensor is lost (-127 C), never run
  // the pumps. All normal on/off decisions come from the gateway.
  if (tempC <= -100.0f) {
    setPumps(false);
  }

  // Drain the ESP-NOW RX queue (commands from the gateway)
  EspNowRxPacket pkt;
  while (espNowQueue && xQueueReceive(espNowQueue, &pkt, 0) == pdTRUE) {
    IncomingMessage msg;
    if (JsonProtocol::decode((const char*)pkt.data, pkt.len, msg)) {
      if (msg.type == MsgType::Cmd) handleCmd(msg);
    }
  }

  // Periodic telemetry to gateway (see docs/api.md)
  if (millis() - lastSendMs >= TELEMETRY_S * 1000UL) {
    lastSendMs = millis();
    JsonDocument payload;
    payload["water_temp_c"] = tempC;
    payload["pump1"] = pump1.isOn() ? "on" : "off";
    payload["pump2"] = pump2.isOn() ? "on" : "off";
    sendMsg(MsgType::Telemetry, payload);
  }

  delay(100);
}
