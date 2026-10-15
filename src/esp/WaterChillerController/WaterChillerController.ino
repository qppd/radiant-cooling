
#include "Config.h"
#include "TemperatureSensor.h"
#include "SsrOutput.h"
#include "EspNowTransport.h"
#include "JsonProtocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Valid DS18B20 reading window (same -90..60 C window the gateway applies)
static const float kValidLoC = -90.0f;
static const float kValidHiC =  60.0f;

// Outgoing (water leaving the tank) + ingoing (water returning to the tank)
TemperatureSensor outgoingTemp(PIN_TEMP_OUTGOING, 1);
TemperatureSensor ingoingTemp(PIN_TEMP_INGOING, 1);
SsrOutput compressor(PIN_SSR_COMPRESSOR);
SsrOutput pump1(PIN_RELAY_PUMP1);
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
  if (compressor.isOn() == on && pump1.isOn() == on) return;
  compressor.set(on);
  pump1.set(on);
  JsonDocument payload;
  payload["compressor"] = on ? "on" : "off";
  payload["pump1"] = on ? "on" : "off";
  sendMsg(MsgType::State, payload);
  Serial.printf("[chiller] pumps %s\n", on ? "ON" : "OFF");
}

// Coldest valid tank sensor reading - reported as water_temp_c, which the
// gateway uses as one of the cold points for its anti-condensation floor.
float coldestTankC(float outgoingC, float ingoingC) {
  bool outOk = outgoingC > kValidLoC && outgoingC < kValidHiC;
  bool inOk  = ingoingC  > kValidLoC && ingoingC  < kValidHiC;
  if (outOk && inOk) return outgoingC < ingoingC ? outgoingC : ingoingC;
  if (outOk) return outgoingC;
  if (inOk)  return ingoingC;
  return outgoingC; // both invalid - raw value anyway, gateway ignores it
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

// ── Serial test console ──────────────────────────────────────────────────
// Open the Serial Monitor at 115200 baud with "Newline" line ending and type
// one of the commands below (then 'help') to exercise each component by hand.

void printTestHelp() {  Serial.println();
  Serial.println("=== WaterChillerController test console ===");
  Serial.println("help                       - show this menu");
  Serial.println("temps                      - read both DS18B20 tank sensors (outgoing / ingoing)");
  Serial.println("temp outgoing|ingoing      - read one DS18B20 by name (out / in also work)");
  Serial.println("ssr                        - show compressor + pump relay output states");
  Serial.printf ("compressor on|off|toggle   - drive compressor SSR (GPIO %d)\n", PIN_SSR_COMPRESSOR);
  Serial.printf ("pump1 on|off|toggle        - drive pump relay (GPIO %d)\n", PIN_RELAY_PUMP1);
  Serial.println("outputs on|off             - drive both at once + report state to gateway");
  Serial.println("espnow                     - send status frame to gateway (link test)");
  Serial.println("status                     - dump current sensor + output state");
  Serial.println("reboot                     - restart the ESP32");
  Serial.println("note: gateway 'set_pumps' and the sensor fail-safe can override outputs");
  Serial.println();
}

void printTempReading(const char* label, TemperatureSensor& sensor, uint8_t pin) {
  float c = sensor.readC(0);
  if (c <= DEVICE_DISCONNECTED_C) {
    Serial.printf("[test] %s (GPIO%d) = DISCONNECTED (%.1f C) - check sensor / wiring / 4.7k pull-up\n",
                  label, pin, c);
  } else if (c == 85.0f) {
    Serial.printf("[test] %s (GPIO%d) = 85.0 C - power-on value, check supply & pull-up\n", label, pin);
  } else if (c <= kValidLoC || c >= kValidHiC) {
    Serial.printf("[test] %s (GPIO%d) = %.2f C - OUT OF RANGE (loop forces all outputs OFF at <= -100 C)\n",
                  label, pin, c);
  } else {
    Serial.printf("[test] %s (GPIO%d) = %.2f C - OK\n", label, pin, c);
  }
}

void testReadAllTemps() {
  outgoingTemp.requestTemperatures();
  ingoingTemp.requestTemperatures();
  delay(750); // DS18B20 12-bit conversion time

  printTempReading("outgoing", outgoingTemp, PIN_TEMP_OUTGOING);
  printTempReading("ingoing",  ingoingTemp,  PIN_TEMP_INGOING);
}

void driveOutput(SsrOutput& out, const char* label, uint8_t pin, String arg) {
  arg.trim();
  if (arg == "on") {
    out.on();
    Serial.printf("[test] %s (GPIO%d) -> ON\n", label, pin);
    Serial.println("[test] note: gateway 'set_pumps' and the sensor fail-safe can override this");
  } else if (arg == "off") {
    out.off();
    Serial.printf("[test] %s (GPIO%d) -> OFF\n", label, pin);
  } else if (arg == "toggle") {
    out.set(!out.isOn());
    Serial.printf("[test] %s (GPIO%d) -> %s\n", label, pin, out.isOn() ? "ON" : "OFF");
    Serial.println("[test] note: gateway 'set_pumps' and the sensor fail-safe can override this");
  } else {
    Serial.printf("[test] usage: %s on | %s off | %s toggle\n", label, label, label);
  }
}

void printOutputStates() {
  Serial.printf("[test] compressor GPIO%d = %s | pump1 GPIO%d = %s\n",
                PIN_SSR_COMPRESSOR, compressor.isOn() ? "ON" : "OFF",
                PIN_RELAY_PUMP1,    pump1.isOn()     ? "ON" : "OFF");
}

void handleSerialCommand() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "help" || cmd == "?") {
    printTestHelp();

  } else if (cmd == "temps") {
    Serial.println("[test] reading both DS18B20 tank sensors...");
    testReadAllTemps();

  } else if (cmd == "temp") {
    Serial.println("[test] usage: temp outgoing | temp ingoing");

  } else if (cmd.startsWith("temp ")) {
    String arg = cmd.substring(5);
    arg.trim();
    if (arg == "outgoing" || arg == "out") {
      Serial.println("[test] reading outgoing sensor (water leaving the tank)...");
      outgoingTemp.requestTemperatures();
      delay(750);
      printTempReading("outgoing", outgoingTemp, PIN_TEMP_OUTGOING);
    } else if (arg == "ingoing" || arg == "in") {
      Serial.println("[test] reading ingoing sensor (water returning to the tank)...");
      ingoingTemp.requestTemperatures();
      delay(750);
      printTempReading("ingoing", ingoingTemp, PIN_TEMP_INGOING);
    } else {
      Serial.println("[test] usage: temp outgoing | temp ingoing");
    }

  } else if (cmd == "ssr" || cmd == "outputs") {
    printOutputStates();

  } else if (cmd.startsWith("compressor ")) {
    driveOutput(compressor, "compressor", PIN_SSR_COMPRESSOR, cmd.substring(11));

  } else if (cmd.startsWith("pump1 ")) {
    driveOutput(pump1, "pump1", PIN_RELAY_PUMP1, cmd.substring(6));

  } else if (cmd.startsWith("outputs ")) {
    String v = cmd.substring(8);
    v.trim();
    if (v == "on" || v == "off") {
      bool on = (v == "on");
      bool changed = !(compressor.isOn() == on && pump1.isOn() == on);
      setPumps(on); // also sends the state frame to the gateway when something changed
      Serial.printf("[test] outputs -> %s%s\n", on ? "ON" : "OFF",
                    changed ? " (state frame sent to gateway)"
                            : " (already in this state - no state frame sent)");
      printOutputStates();
    } else {
      Serial.println("[test] usage: outputs on | outputs off");
    }

  } else if (cmd == "espnow") {
    Serial.println("[test] sending status frame to gateway...");
    sendStatus();
    Serial.println("[test] expect '[espnow] send OK -> ..' here and '[espnow] rx ... src=chiller' on the gateway");

  } else if (cmd == "status") {
    Serial.println("=== WaterChillerController status ===");
    testReadAllTemps();
    printOutputStates();

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

  outgoingTemp.begin();
  ingoingTemp.begin();
  compressor.begin();
  pump1.begin();

  if (!espNow.begin()) {
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

  outgoingTemp.requestTemperatures();
  ingoingTemp.requestTemperatures();
  delay(750);
  float outgoingC = outgoingTemp.readC(0);
  float ingoingC  = ingoingTemp.readC(0);
  float waterC    = coldestTankC(outgoingC, ingoingC);

  // Fail-safe: a dead tank sensor means we can't prove the water is warm
  // enough, so both outputs go off until a valid reading returns.
  if (outgoingC <= -100.0f || ingoingC <= -100.0f) {
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
    payload["ingoing_temp_c"]  = ingoingC;
    payload["water_temp_c"]    = waterC; // coldest valid tank reading
    payload["compressor"]      = compressor.isOn() ? "on" : "off";
    payload["pump1"]           = pump1.isOn() ? "on" : "off";
    sendMsg(MsgType::Telemetry, payload);
  }

  delay(100);
}
