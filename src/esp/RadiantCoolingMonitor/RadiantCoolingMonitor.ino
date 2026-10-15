
#include "Config.h"
#include "TemperatureSensor.h"
#include "WifiProvisioner.h"
#include "EspNowTransport.h"
#include "JsonProtocol.h"
#include "FirebaseSync.h"
#include "WeatherApi.h"
#include "ClimateControl.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <time.h>

// 4x ceiling DS18B20 - each sensor on its own 1-Wire bus (see PINS_CONFIG.h)
TemperatureSensor ceiling1(PIN_TEMP_CEILING_1, 1);
TemperatureSensor ceiling2(PIN_TEMP_CEILING_2, 1);
TemperatureSensor ceiling3(PIN_TEMP_CEILING_3, 1);
TemperatureSensor ceiling4(PIN_TEMP_CEILING_4, 1);

static const uint8_t kCeilingPins[TEMP_COUNT] = {
  PIN_TEMP_CEILING_1, PIN_TEMP_CEILING_2, PIN_TEMP_CEILING_3, PIN_TEMP_CEILING_4
};

TemperatureSensor* ceilingSensor(uint8_t index) {
  switch (index) {
    case 0: return &ceiling1;
    case 1: return &ceiling2;
    case 2: return &ceiling3;
    case 3: return &ceiling4;
    default: return nullptr;
  }
}

void beginCeilings() {
  ceiling1.begin();
  ceiling2.begin();
  ceiling3.begin();
  ceiling4.begin();
}

void requestCeilingTemps() {
  ceiling1.requestTemperatures();
  ceiling2.requestTemperatures();
  ceiling3.requestTemperatures();
  ceiling4.requestTemperatures();
}

void readCeilingTemps(float* out) {
  for (uint8_t i = 0; i < TEMP_COUNT; i++) {
    TemperatureSensor* s = ceilingSensor(i);
    out[i] = s ? s->readC(0) : DEVICE_DISCONNECTED_C;
  }
}

WifiProvisioner wifi(WIFI_AP_NAME, PIN_WIFI_RESET_BUTTON);
EspNowTransport espNow;
FirebaseSync cloud;
WeatherApi weather(WEATHER_LOCATION);

unsigned long lastPublishMs = 0;
unsigned long lastWeatherMs = 0;
unsigned long lastWeatherUpdateMs = 0;
uint32_t seq = 0;

bool weatherValid = false;
float outdoorHumidityPct = 0.0f;
ControlInputs inputs;
ControlParams  params;
bool pumpsOn = false;

// Control stream state tracking (avoid re-sending unchanged values)
String lastCtrlMode = "";
float  lastCtrlSetpoint = -999.0f;


typedef struct {
  uint8_t mac[6];
  uint8_t data[250];
  size_t  len;
} EspNowRxPacket;
QueueHandle_t espNowQueue;


uint32_t nowUnix() {
  time_t t = time(nullptr);
  return t > 1000000000UL ? (uint32_t)t : (uint32_t)(millis() / 1000UL);
}

void sendTo(const uint8_t* mac, MsgType type, const JsonDocument& payload) {
  char buf[250];
  size_t n = JsonProtocol::encode(type, DEVICE_ID, ++seq, payload, buf, sizeof(buf));
  if (n > 0 && n < sizeof(buf)) espNow.sendTo(mac, (const uint8_t*)buf, n);
}

void sendPumpCmd(bool on) {
  JsonDocument payload;
  payload["cmd"]   = "set_pumps";
  payload["value"] = on ? "on" : "off";
  sendTo(PEER_CHILLER, MsgType::Cmd, payload);
  Serial.printf("[gateway] cmd chiller: set_pumps %s\n", on ? "on" : "off");
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

void handleIncoming(IncomingMessage& msg) {
  Serial.printf("[espnow] rx type=%d src=%s seq=%u\n",
                (int)msg.type, msg.src.c_str(), msg.seq);

  if (msg.type == MsgType::Telemetry) {
    if (msg.src == "dh") {
      inputs.indoorTempC       = msg.data["temp_c"] | -100.0f;
      inputs.indoorHumidityPct = msg.data["humidity_pct"] | 0.0f;
    } else if (msg.src == "chiller") {
      inputs.waterTempC = msg.data["water_temp_c"] | -100.0f;
    }
    msg.data["ts"] = nowUnix();
    String path = String(RadiantFirebaseConfig::getTelemetryBasePath()) +
                  "/" + msg.src + "/latest";
    char json[512];
    serializeJson(msg.data, json, sizeof(json));
    cloud.setJson(path.c_str(), json);
  } else if (msg.type == MsgType::State) {
    String path = String(RadiantFirebaseConfig::getStateBasePath()) + "/" + msg.src;
    char json[256];
    serializeJson(msg.data, json, sizeof(json));
    cloud.setJson(path.c_str(), json);
  } else if (msg.type == MsgType::Status && msg.src == "chiller") {
    sendPumpCmd(pumpsOn);
  }
}

// ── Config Stream Callback (SmartFan MultiPathStream style) ──
void onConfigStream(MultiPathStream stream) {
  Serial.printf("[config] %s -> %s\n", stream.dataPath.c_str(), stream.value.c_str());

  // Handle control params
  if (stream.get("/control/params/comfort_setpoint_c")) {
    params.comfortSetpointC = stream.value.toFloat();
    Serial.printf("[config] comfort_setpoint_c = %.1f\n", params.comfortSetpointC);
  }
  if (stream.get("/control/params/dewpoint_margin_c")) {
    params.dewPointMarginC = stream.value.toFloat();
    Serial.printf("[config] dewpoint_margin_c = %.1f\n", params.dewPointMarginC);
  }
  if (stream.get("/control/params/weather_cool_temp_c")) {
    params.weatherCoolTempC = stream.value.toFloat();
    Serial.printf("[config] weather_cool_temp_c = %.1f\n", params.weatherCoolTempC);
  }

  // Handle weather key update
  if (stream.get("/weather_key")) {
    String key = stream.value;
    key.replace("\"", "");
    key.trim();
    if (key.length() > 0) {
      weather.setKey(key.c_str());
      Serial.println("[config] weather key updated");
    }
  }

  // Handle dehumidifier config
  if (stream.get("/dh/humidity_setpoint_pct") || stream.get("/dh/humidity_deadband_pct")) {
    // Need to fetch the full dh node — for individual field updates, just forward
    Serial.println("[config] DH config change detected");
  }
  if (stream.get("/dh")) {
    // Full dh object received
    String val = stream.value;
    val.replace("\"", "");

    // Parse and forward to dehumidifier
    FirebaseJson json;
    FirebaseJsonData result;
    json.setJsonData(stream.value);

    float setpoint = 55.0f;
    float deadband = 5.0f;
    if (json.get(result, "humidity_setpoint_pct")) setpoint = result.to<float>();
    if (json.get(result, "humidity_deadband_pct")) deadband = result.to<float>();

    JsonDocument payload;
    payload["humidity_setpoint_pct"] = setpoint;
    payload["humidity_deadband_pct"] = deadband;
    sendTo(PEER_DEHUM, MsgType::Config, payload);
    Serial.printf("[config] Forwarded DH config: setpoint=%.1f deadband=%.1f\n", setpoint, deadband);
  }
}

// ── Control Stream Callback (SmartFan MultiPathStream style) ──
void onControlStream(MultiPathStream stream) {
  Serial.printf("[control] %s -> %s\n", stream.dataPath.c_str(), stream.value.c_str());

  // Handle pumps on/off command
  if (stream.get("/pumps")) {
    String val = stream.value;
    val.replace("\"", "");
    val.trim();

    bool cmdOn = (val == "on" || val == "true" || val == "1");
    bool cmdOff = (val == "off" || val == "false" || val == "0");

    if (cmdOn && !pumpsOn) {
      Serial.println("[control] Pumps ON commanded from dashboard");
      pumpsOn = true;
      sendPumpCmd(true);
    } else if (cmdOff && pumpsOn) {
      Serial.println("[control] Pumps OFF commanded from dashboard");
      pumpsOn = false;
      sendPumpCmd(false);
    }
  }

  // Handle mode change (auto/manual)
  if (stream.get("/mode")) {
    String newMode = stream.value;
    newMode.replace("\"", "");
    newMode.trim();

    if (newMode != lastCtrlMode) {
      Serial.printf("[control] Mode changed: '%s' -> '%s'\n",
                    lastCtrlMode.c_str(), newMode.c_str());
      lastCtrlMode = newMode;

      if (newMode == "manual") {
        Serial.println("[control] Switched to MANUAL mode");
      } else {
        Serial.println("[control] Switched to AUTO mode");
      }
    }
  }

  // Handle comfort setpoint change
  if (stream.get("/comfort_setpoint_c")) {
    float val = stream.value.toFloat();
    if (val > 0 && val != lastCtrlSetpoint) {
      Serial.printf("[control] Comfort setpoint changed: %.1f -> %.1f\n",
                    lastCtrlSetpoint, val);
      lastCtrlSetpoint = val;
      params.comfortSetpointC = val;
    }
  }

  // Handle dewpoint margin change
  if (stream.get("/dewpoint_margin_c")) {
    float val = stream.value.toFloat();
    if (val > 0) {
      Serial.printf("[control] Dewpoint margin changed to: %.1f\n", val);
      params.dewPointMarginC = val;
    }
  }

  // Handle weather cool temp change
  if (stream.get("/weather_cool_temp_c")) {
    float val = stream.value.toFloat();
    if (val > 0) {
      Serial.printf("[control] Weather cool temp changed to: %.1f\n", val);
      params.weatherCoolTempC = val;
    }
  }

  // Handle dehumidifier setpoints forwarded to DH controller
  if (stream.get("/dh")) {
    FirebaseJson json;
    FirebaseJsonData result;
    json.setJsonData(stream.value);

    float setpoint = 55.0f;
    float deadband = 5.0f;
    if (json.get(result, "humidity_setpoint_pct")) setpoint = result.to<float>();
    if (json.get(result, "humidity_deadband_pct")) deadband = result.to<float>();

    JsonDocument payload;
    payload["humidity_setpoint_pct"] = setpoint;
    payload["humidity_deadband_pct"] = deadband;
    sendTo(PEER_DEHUM, MsgType::Config, payload);
    Serial.println("[control] Forwarded DH config to dehumidifier");
  }
}

// ── Serial test console ──────────────────────────────────────────────────
// Open the Serial Monitor at 115200 baud with "Newline" line ending and type
// one of the commands below (then 'help') to exercise each component by hand.

void handleSendResult(const uint8_t* mac, bool success) {
  Serial.printf("[espnow] tx %s -> %02X:%02X:%02X:%02X:%02X:%02X\n",
                success ? "OK  " : "FAIL",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void printTestHelp() {
  Serial.println();
  Serial.println("=== RadiantCoolingMonitor test console ===");
  Serial.println("help                  - show this menu");
  Serial.println("temps                 - read all 4 ceiling DS18B20 sensors (1 bus each)");
  Serial.println("temp <0-3>            - read one ceiling sensor by index");
  Serial.printf("button                - read WiFi reset button (GPIO %d) once\n", PIN_WIFI_RESET_BUTTON);
  Serial.println("button test           - watch button press/release edges for 10 s");
  Serial.println("wifi                  - WiFi status (SSID / IP / RSSI / MAC)");
  Serial.println("wx                    - fetch outdoor weather now");
  Serial.println("mode manual|auto      - switch control mode");
  Serial.println("pumps on|off          - send pump cmd to chiller (ESP-NOW TX test)");
  Serial.println("espnow chiller|dh     - send test ping over ESP-NOW");
  Serial.println("status                - dump current inputs / control state");
  Serial.println("reboot                - restart the ESP32");
  Serial.println();
}

// Read the 4 ceiling DS18B20 sensors (one 1-Wire bus each) - all, or one index.
void testReadTemps(uint8_t onlyIndex, bool single) {
  if (single) {
    TemperatureSensor* req = ceilingSensor(onlyIndex);
    if (!req) return;
    req->requestTemperatures();
    delay(750); // DS18B20 12-bit conversion time
  } else {
    requestCeilingTemps();
    delay(750);
  }

  for (uint8_t i = 0; i < TEMP_COUNT; i++) {
    if (single && i != onlyIndex) continue;
    TemperatureSensor* s = ceilingSensor(i);
    if (!s) continue;
    float c = s->readC(0);
    if (c <= DEVICE_DISCONNECTED_C) {
      Serial.printf("[test] ceiling_%d (GPIO%d) = DISCONNECTED (%.1f C) - check sensor / wiring / 4.7k pull-up\n",
                    i + 1, kCeilingPins[i], c);
    } else if (c == 85.0f) {
      Serial.printf("[test] ceiling_%d (GPIO%d) = 85.0 C - power-on value, check supply & pull-up\n",
                    i + 1, kCeilingPins[i]);
    } else if (c <= kTempValidLoC || c >= kTempValidHiC) {
      Serial.printf("[test] ceiling_%d (GPIO%d) = %.2f C - OUT OF RANGE\n",
                    i + 1, kCeilingPins[i], c);
    } else {
      Serial.printf("[test] ceiling_%d (GPIO%d) = %.2f C - OK\n",
                    i + 1, kCeilingPins[i], c);
    }
  }
}

void handleSerialCommand() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "help" || cmd == "?") {
    printTestHelp();

  } else if (cmd == "temps") {
    Serial.println("[test] reading all 4 ceiling DS18B20 sensors...");
    testReadTemps(0, false);

  } else if (cmd == "temp") {
    Serial.println("[test] usage: temp <0-3>  (e.g. temp 2)");

  } else if (cmd.startsWith("temp ")) {
    String arg = cmd.substring(5);
    arg.trim();
    if (arg.length() > 0 && arg.charAt(0) >= '0' && arg.charAt(0) <= '9') {
      int idx = arg.toInt();
      if (idx >= 0 && idx < TEMP_COUNT) {
        Serial.printf("[test] reading ceiling_%d sensor...\n", idx + 1);
        testReadTemps((uint8_t)idx, true);
      } else {
        Serial.printf("[test] bad index %d - valid range 0..%d\n", idx, (int)TEMP_COUNT - 1);
      }
    } else {
      Serial.println("[test] usage: temp <0-3>  (e.g. temp 2)");
    }

  } else if (cmd == "button") {
    int v = digitalRead(PIN_WIFI_RESET_BUTTON);
    Serial.printf("[test] button GPIO%d = %s (%s)\n", PIN_WIFI_RESET_BUTTON,
                  v == LOW ? "LOW" : "HIGH",
                  v == LOW ? "PRESSED" : "released");

  } else if (cmd == "button test" || cmd == "button watch") {
    Serial.printf("[test] watching GPIO%d for 10 s - press / release the button now\n",
                  PIN_WIFI_RESET_BUTTON);
    Serial.println("[test] (WiFi erase only triggers AFTER this test, if held 3 s)");
    unsigned long startMs = millis();
    int last = digitalRead(PIN_WIFI_RESET_BUTTON);
    unsigned long pressMs = 0;
    Serial.printf("[test] start state: %s\n", last == LOW ? "PRESSED" : "released");
    while (millis() - startMs < 10000UL) {
      int v = digitalRead(PIN_WIFI_RESET_BUTTON);
      if (v != last) {
        if (v == LOW) {
          pressMs = millis();
          Serial.println("[test] edge: PRESSED");
        } else {
          Serial.printf("[test] edge: RELEASED after %lu ms\n", millis() - pressMs);
        }
        last = v;
      }
      delay(10);
    }
    if (last == LOW) {
      Serial.printf("[test] watch ended - button still held %lu ms (release it now!)\n",
                    millis() - pressMs);
    } else {
      Serial.println("[test] watch finished");
    }

  } else if (cmd == "wifi") {
    Serial.printf("[test] wifi %s | ssid=%s | ip=%s | rssi=%d dBm | mac=%s\n",
                  wifi.connected() ? "CONNECTED" : "DISCONNECTED",
                  WiFi.SSID().c_str(),
                  wifi.localIP().c_str(),
                  WiFi.RSSI(),
                  WiFi.macAddress().c_str());

  } else if (cmd == "wx") {
    Serial.println("[test] fetching outdoor weather...");
    if (!weather.hasKey()) {
      Serial.println("[test] warning: no weather API key stored (set /weather_key in Firebase)");
    }
    WeatherConditions w = weather.fetch();
    if (w.ok) {
      inputs.outdoorTempC      = w.tempC;
      inputs.outdoorDewPointC  = w.dewPointC;
      outdoorHumidityPct       = w.humidityPct;
      weatherValid             = true;
      lastWeatherMs            = millis();
      lastWeatherUpdateMs      = millis();
      Serial.printf("[test] weather OK: temp=%.1f C humidity=%.1f %% dewpoint=%.1f C\n",
                    w.tempC, w.humidityPct, w.dewPointC);
    } else {
      Serial.println("[test] weather fetch FAILED (key missing, invalid, or no internet)");
    }

  } else if (cmd.startsWith("mode ")) {
    String m = cmd.substring(5);
    m.trim();
    if (m == "manual" || m == "auto") {
      lastCtrlMode = m;
      Serial.printf("[test] mode -> %s\n", m.c_str());
      if (m == "manual") Serial.println("[test] auto pump control is now suspended");
    } else {
      Serial.println("[test] usage: mode manual | mode auto");
    }

  } else if (cmd.startsWith("pumps ")) {
    String v = cmd.substring(6);
    v.trim();
    if (v == "on" || v == "off") {
      bool on = (v == "on");
      pumpsOn = on;
      sendPumpCmd(on);
      if (lastCtrlMode != "manual") {
        Serial.println("[test] note: mode is AUTO - control loop may override this; send 'mode manual' first");
      }
    } else {
      Serial.println("[test] usage: pumps on | pumps off");
    }

  } else if (cmd.startsWith("espnow ")) {
    String peer = cmd.substring(7);
    peer.trim();
    const uint8_t* mac = nullptr;
    if      (peer == "chiller") mac = PEER_CHILLER;
    else if (peer == "dh")      mac = PEER_DEHUM;
    if (!mac) {
      Serial.println("[test] usage: espnow chiller | espnow dh");
    } else {
      JsonDocument payload;
      payload["cmd"] = "ping"; // peers log "unknown cmd: ping" when it arrives
      Serial.printf("[test] sending ESP-NOW ping to %s...\n", peer.c_str());
      sendTo(mac, MsgType::Cmd, payload);
      Serial.println("[test] expect '[espnow] tx OK -> ..' here and 'unknown cmd: ping' on the peer");
    }

  } else if (cmd == "status") {
    Serial.println("=== RadiantCoolingMonitor status ===");
    Serial.printf("mode=%s  pumps=%s  weather=%s\n",
                  lastCtrlMode.length() > 0 ? lastCtrlMode.c_str() : "auto",
                  pumpsOn ? "on" : "off",
                  weatherValid ? "valid" : "stale");
    Serial.printf("supply=%.2f C  return=%.2f C  coldest_pipe=%.2f C  water=%.2f C\n",
                  inputs.supplyTempC, inputs.returnTempC, inputs.coldestPipeC, inputs.waterTempC);
    Serial.printf("indoor=%.2f C %.1f %%RH  outdoor=%.2f C dew=%.2f C\n",
                  inputs.indoorTempC, inputs.indoorHumidityPct,
                  inputs.outdoorTempC, inputs.outdoorDewPointC);
    Serial.printf("setpoint=%.1f C  dewpoint_margin=%.1f C  weather_cool=%.1f C\n",
                  params.comfortSetpointC, params.dewPointMarginC, params.weatherCoolTempC);
    Serial.printf("wifi=%s  ip=%s\n",
                  wifi.connected() ? "up" : "down",
                  wifi.localIP().c_str());

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

  beginCeilings();

  if (!wifi.begin()) {
    Serial.println("WiFi not connected - connect to AP 'RadiantCooling-AP' to configure");
    ESP.restart();
  }
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  if (!espNow.begin()) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  espNow.addPeer(PEER_CHILLER);
  espNow.addPeer(PEER_DEHUM);
  espNow.onReceive(handleMessage);
  espNow.onSend(handleSendResult); // serial TX feedback for ESP-NOW tests

  Serial.println("[test] type 'help' for the serial test console");

  // Register stream handlers before begin()
  cloud.setConfigStreamHandler(onConfigStream);
  cloud.setControlStreamHandler(onControlStream);

  // Initialize Firebase (handles WiFi check, auth, streams)
  cloud.begin();
}

void loop() {
  handleSerialCommand(); // manual component test console (Serial.readStringUntil)

  wifi.handleResetButton(WIFI_RESET_HOLD_MS);
  wifi.reconnectIfLost();

  cloud.loop();

  requestCeilingTemps();
  delay(750);

  float pipeTemps[TEMP_COUNT];
  readCeilingTemps(pipeTemps);
  inputs.supplyTempC  = pipeTemps[IDX_SUPPLY];
  inputs.returnTempC  = pipeTemps[IDX_RETURN];
  inputs.coldestPipeC = ClimateControl::coldestValidC(pipeTemps, TEMP_COUNT);

  EspNowRxPacket pkt;
  while (espNowQueue && xQueueReceive(espNowQueue, &pkt, 0) == pdTRUE) {
    IncomingMessage msg;
    if (JsonProtocol::decode((const char*)pkt.data, pkt.len, msg)) {
      handleIncoming(msg);
    }
  }

  if (weather.hasKey() && millis() - lastWeatherMs >= WEATHER_POLL_S * 1000UL) {
    lastWeatherMs = millis();
    WeatherConditions wx = weather.fetch();
    if (wx.ok) {
      inputs.outdoorTempC      = wx.tempC;
      inputs.outdoorDewPointC  = wx.dewPointC;
      outdoorHumidityPct       = wx.humidityPct;
      weatherValid = true;
      lastWeatherUpdateMs = millis();
      Serial.printf("[weather] ok: temp=%.1f dewpoint=%.1f\n",
                    inputs.outdoorTempC, inputs.outdoorDewPointC);
    } else {
      Serial.println("[weather] fetch failed (key missing or invalid)");
    }
  }

  if (weatherValid && millis() - lastWeatherUpdateMs >= WEATHER_STALE_S * 1000UL) {
    weatherValid = false;
    inputs.outdoorTempC      = -100.0f;
    inputs.outdoorDewPointC  = -100.0f;
    outdoorHumidityPct       = -100.0f;
    Serial.println("[weather] stale - indoor dew point only");
  }

  ControlDecision d = ClimateControl::decidePumps(inputs, params, pumpsOn);

  // Only auto-control pumps when in auto mode (or no mode set yet)
  if (lastCtrlMode != "manual") {
    if (d.pumpsOn != pumpsOn) {
      pumpsOn = d.pumpsOn;
      sendPumpCmd(pumpsOn);
    }
  }

  if (millis() - lastPublishMs >= TELEMETRY_S * 1000UL) {
    lastPublishMs = millis();

    JsonDocument tel;
    JsonArray temps = tel["temps_c"].to<JsonArray>();
    for (uint8_t i = 0; i < TEMP_COUNT; i++) temps.add(pipeTemps[i]);
    tel["supply_c"]       = inputs.supplyTempC;
    tel["return_c"]       = inputs.returnTempC;
    tel["coldest_pipe_c"] = inputs.coldestPipeC;
    tel["delta_t_c"] = (inputs.returnTempC > kTempValidLoC && inputs.returnTempC < kTempValidHiC &&
                        inputs.supplyTempC > kTempValidLoC && inputs.supplyTempC < kTempValidHiC)
                            ? inputs.returnTempC - inputs.supplyTempC : 0.0f;
    tel["outdoor_temp_c"]      = inputs.outdoorTempC;
    tel["outdoor_dewpoint_c"]  = inputs.outdoorDewPointC;
    tel["outdoor_humidity_pct"] = outdoorHumidityPct;
    tel["dew_point_c"]    = d.refDewPointC;
    tel["water_floor_c"]  = d.waterFloorC;
    tel["pumps"]          = pumpsOn ? "on" : "off";
    tel["mode"]           = lastCtrlMode.length() > 0 ? lastCtrlMode : "auto";
    tel["ts"]             = nowUnix();
    char json[512];
    serializeJson(tel, json, sizeof(json));
    cloud.setJson(
      String(RadiantFirebaseConfig::getTelemetryBasePath()) + "/monitor/latest",
      json);

    JsonDocument hb;
    hb["online"]    = true;
    hb["firmware"]  = "0.1.0";
    hb["channel"]   = WiFi.channel();
    hb["device_id"] = SYSTEM_ID;
    hb["ts"]        = nowUnix();
    char hbJson[256];
    serializeJson(hb, hbJson, sizeof(hbJson));
    cloud.setJson(RadiantFirebaseConfig::getHeartbeatPath(), hbJson);

    JsonDocument dev;
    dev["online"]    = true;
    dev["firmware"]  = "0.1.0";
    dev["channel"]   = WiFi.channel();
    dev["device_id"] = SYSTEM_ID;
    dev["ts"]        = nowUnix();
    char devJson[256];
    serializeJson(dev, devJson, sizeof(devJson));
    cloud.setJson(
      String(RadiantFirebaseConfig::getDevicesBasePath()) + "/" + SYSTEM_ID,
      devJson);
  }

  delay(100);
}
