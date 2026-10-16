
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

// ── Serial test console ──────────────────────────────────────────────────
// Open the Serial Monitor at 115200 baud with "Newline" line ending and type
// one of the commands below (then 'help') to exercise each component by hand.

void printTestHelp() {
  Serial.println();
  Serial.println("=== DehumidifierController test console ===");
  Serial.println("help                  - show this menu");
  Serial.println("dht                   - read DHT22 once (temp + humidity)");
  Serial.println("dht test              - read DHT22 5x, 2 s apart (stability check)");
  Serial.println("ssr                   - show SSR / dehumidifier output state");
  Serial.println("ssr on|off|toggle     - drive the SSR output by hand");
  Serial.println("setpoint <0-100>      - set humidity setpoint %RH");
  Serial.println("deadband <0-30>       - set humidity deadband %RH");
  Serial.println("enable | disable      - enable / disable control (disable = SSR stays off)");
  Serial.println("espnow                - send status frame to gateway (link test)");
  Serial.println("status                - dump current state");
  Serial.println("reboot                - restart the ESP32");
  Serial.println();
}

void testReadDht() {
  float t, h;
  if (roomClimate.read(t, h)) {
    Serial.printf("[test] DHT22: %.1f C, %.1f %%RH - OK\n", t, h);
    if (h < 0.0f || h > 100.0f) {
      Serial.println("[test] humidity out of range - possible wiring fault");
    }
  } else {
    Serial.printf("[test] DHT22: READ FAILED (NaN) - check GPIO%d wiring & 10k pull-up\n",
                  PIN_DHT22);
  }
}

void handleSerialCommand() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "help" || cmd == "?") {
    printTestHelp();

  } else if (cmd == "dht") {
    Serial.println("[test] reading DHT22...");
    testReadDht();

  } else if (cmd == "dht test") {
    Serial.println("[test] DHT22 stability test: 5 samples, 2 s apart");
    for (int i = 1; i <= 5; i++) {
      Serial.printf("[test] sample %d/5: ", i);
      testReadDht();
      if (i < 5) delay(2000);
    }

  } else if (cmd == "ssr") {
    Serial.printf("[test] SSR GPIO%d = %s (%s)\n", PIN_SSR_DEHUM,
                  dehumidifier.isOn() ? "HIGH" : "LOW",
                  dehumidifier.isOn() ? "dehumidifier ON" : "dehumidifier OFF");

  } else if (cmd.startsWith("ssr ")) {
    String v = cmd.substring(4);
    v.trim();
    if (v == "on") {
      dehumidifier.on();
      Serial.println("[test] SSR -> ON (dehumidifier should run)");
      if (systemEnabled) Serial.println("[test] note: control loop is ENABLED and may override this - send 'disable' first");
    } else if (v == "off") {
      dehumidifier.off();
      Serial.println("[test] SSR -> OFF (dehumidifier should stop)");
    } else if (v == "toggle") {
      dehumidifier.set(!dehumidifier.isOn());
      Serial.printf("[test] SSR -> %s\n", dehumidifier.isOn() ? "ON" : "OFF");
      if (systemEnabled) Serial.println("[test] note: control loop is ENABLED and may override this - send 'disable' first");
    } else {
      Serial.println("[test] usage: ssr on | ssr off | ssr toggle");
    }

  } else if (cmd.startsWith("setpoint ")) {
    String arg = cmd.substring(9);
    arg.trim();
    if (arg.length() > 0 && arg.charAt(0) >= '0' && arg.charAt(0) <= '9') {
      float v = arg.toFloat();
      if (v >= 0.0f && v <= 100.0f) {
        HUMIDITY_SETPOINT_PCT = v;
        Serial.printf("[test] setpoint -> %.1f %%RH\n", HUMIDITY_SETPOINT_PCT);
      } else {
        Serial.println("[test] usage: setpoint <0-100>");
      }
    } else {
      Serial.println("[test] usage: setpoint <0-100>");
    }

  } else if (cmd.startsWith("deadband ")) {
    String arg = cmd.substring(9);
    arg.trim();
    if (arg.length() > 0 && arg.charAt(0) >= '0' && arg.charAt(0) <= '9') {
      float v = arg.toFloat();
      if (v >= 0.0f && v <= 30.0f) {
        HUMIDITY_DEADBAND_PCT = v;
        Serial.printf("[test] deadband -> %.1f %%RH\n", HUMIDITY_DEADBAND_PCT);
      } else {
        Serial.println("[test] usage: deadband <0-30>");
      }
    } else {
      Serial.println("[test] usage: deadband <0-30>");
    }

  } else if (cmd == "enable") {
    systemEnabled = true;
    Serial.println("[test] control ENABLED - dehumidifier follows humidity again");

  } else if (cmd == "disable") {
    systemEnabled = false;
    dehumidifier.off();
    Serial.println("[test] control DISABLED - dehumidifier forced OFF (safe for SSR tests)");

  } else if (cmd == "espnow") {
    Serial.println("[test] sending status frame to gateway...");
    sendStatus();
    Serial.println("[test] expect '[espnow] tx OK -> ..' here and '[espnow] rx ... src=dh' on the gateway");

  } else if (cmd == "status") {
    Serial.println("=== DehumidifierController status ===");
    Serial.printf("temp=%.1f C  humidity=%.1f %%RH  read_failures=%u  failsafe=%s\n",
                  lastTempC, lastHumidityPct, readFailures,
                  failSafeTripped ? "TRIPPED" : "clear");
    Serial.printf("setpoint=%.1f %%RH  deadband=%.1f %%RH  control=%s  ssr=%s\n",
                  HUMIDITY_SETPOINT_PCT, HUMIDITY_DEADBAND_PCT,
                  systemEnabled ? "enabled" : "disabled",
                  dehumidifier.isOn() ? "ON" : "OFF");
    // Compare with the gateway's `wifi` channel - both must match.
    Serial.printf("espnow channel=%u (pinned %u)  gateway=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  EspNowTransport::channel(), ESP_NOW_CHANNEL,
                  GATEWAY_MAC[0], GATEWAY_MAC[1], GATEWAY_MAC[2],
                  GATEWAY_MAC[3], GATEWAY_MAC[4], GATEWAY_MAC[5]);

  } else if (cmd == "reboot" || cmd == "restart") {
    Serial.println("[test] rebooting...");
    Serial.flush();
    ESP.restart();

  } else {
    Serial.printf("[test] unknown command '%s' - type 'help' for the list\n", cmd.c_str());
  }
}

void setup() {
  Serial.begin(115200);

  espNowQueue = xQueueCreate(8, sizeof(EspNowRxPacket));

  roomClimate.begin();
  dehumidifier.begin();

  if (!espNow.begin(ESP_NOW_CHANNEL)) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  espNow.addPeer(GATEWAY_MAC);
  espNow.onReceive(handleMessage);
  espNow.onSend(handleSendResult);

  sendStatus();

  Serial.println("[test] type 'help' for the serial test console");
}

void loop() {
  handleSerialCommand(); // manual component test console (Serial.readStringUntil)

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
