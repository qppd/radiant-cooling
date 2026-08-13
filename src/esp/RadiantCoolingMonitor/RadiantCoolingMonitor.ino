/*
 * RadiantCoolingMonitor.ino
 *
 * ESP32 GATEWAY - reads 6x DS18B20 temperature sensors, receives ESP-NOW
 * packets from the chiller and dehumidifier boards, receives outdoor
 * weather from the Flutter app via Firebase, and syncs all sensor data to
 * Firebase Realtime Database. Also computes the water chiller pump control
 * (weather + sensors + condensation).
 *
 * Firebase is two-way (Mobizt FirebaseClient, async):
 *   - SAVE:    telemetry/state written to radiant/telemetry/* via setJson()
 *   - RECEIVE: radiant/config streamed in real time - app changes arrive
 *              immediately and are applied or forwarded over ESP-NOW
 *
 * WiFi is provisioned with the WiFiManager library (captive portal on first
 * boot; hold the reset button ~3 s to erase saved SSID/password).
 *
 * This file is glue only. All component/library code is encapsulated:
 *   Config.h            - board configuration (MACs, credentials, constants); includes PINS_CONFIG.h
 *   PINS_CONFIG.h       - pin assignments (1-Wire, WiFi reset button)
 *   TemperatureSensor   - wraps OneWire + DallasTemperature (6x DS18B20)
 *   WifiProvisioner     - wraps WiFiManager (captive-portal provisioning)
 *   EspNowTransport     - wraps WiFi + esp_now (register peers, send/receive)
 *   JsonProtocol        - wraps ArduinoJson (ESP-NOW message envelope)
 *   FirebaseSync        - wraps FirebaseClient (set/update + realtime stream)
 *   ClimateControl      - dew point + pump decision (control computation)
 *
 * See docs/ for architecture, API and control logic.
 */

#include "Config.h"
#include "TemperatureSensor.h"
#include "WifiProvisioner.h"
#include "EspNowTransport.h"
#include "JsonProtocol.h"
#include "FirebaseSync.h"
#include "ClimateControl.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <time.h>

// ---- Components ----
TemperatureSensor loopTemps(PIN_ONE_WIRE, TEMP_COUNT);
WifiProvisioner wifi(WIFI_AP_NAME, PIN_WIFI_RESET_BUTTON);
EspNowTransport espNow;
FirebaseSync cloud;

// ---- Telemetry state ----
unsigned long lastPublishMs = 0;
unsigned long lastWeatherUpdateMs = 0;  // when outdoor weather last arrived from the app
uint32_t seq = 0;

// ---- Control state ----
bool weatherValid = false;   // outdoor weather received from the app (config/weather stream)
ControlInputs inputs;        // latest values from all sources
ControlParams  params;       // defaults; updated from the config stream
bool pumpsOn = false;

// ESP-NOW receive -> Firebase write is decoupled via a FreeRTOS queue:
// the receive callback ONLY enqueues raw bytes (never blocks / decodes /
// calls Firebase inside the callback); loop() drains and processes.

typedef struct {
  uint8_t mac[6];
  uint8_t data[250];         // ESP-NOW max payload
  size_t  len;
} EspNowRxPacket;
QueueHandle_t espNowQueue;

// ---- Helpers ----

// Unix time once NTP syncs; uptime seconds as a fallback.
uint32_t nowUnix() {
  time_t t = time(nullptr);
  return t > 1000000000UL ? (uint32_t)t : (uint32_t)(millis() / 1000UL);
}

// Encode + send a JSON message to one peer.
void sendTo(const uint8_t* mac, MsgType type, const JsonDocument& payload) {
  char buf[250];
  size_t n = JsonProtocol::encode(type, DEVICE_ID, ++seq, payload, buf, sizeof(buf));
  // n == maxLen means the JSON was truncated (serializeJson caps at maxLen).
  if (n > 0 && n < sizeof(buf)) espNow.sendTo(mac, (const uint8_t*)buf, n);
}

// Gateway -> chiller: set_pumps command.
void sendPumpCmd(bool on) {
  JsonDocument payload;
  payload["cmd"]   = "set_pumps";
  payload["value"] = on ? "on" : "off";
  sendTo(PEER_CHILLER, MsgType::Cmd, payload);
  Serial.printf("[gateway] cmd chiller: set_pumps %s\n", on ? "on" : "off");
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

// ---- Process one decoded message from a peer ----
void handleIncoming(IncomingMessage& msg) {
  Serial.printf("[espnow] rx type=%d src=%s seq=%u\n",
                (int)msg.type, msg.src.c_str(), msg.seq);

  if (msg.type == MsgType::Telemetry) {
    // Cache the latest peer readings for the chiller control computation.
    if (msg.src == "dh") {
      inputs.indoorTempC       = msg.data["temp_c"] | -100.0f;
      inputs.indoorHumidityPct = msg.data["humidity_pct"] | 0.0f;
    } else if (msg.src == "chiller") {
      inputs.waterTempC = msg.data["water_temp_c"] | -100.0f;
    }
    // Forward to Firebase: radiant/telemetry/<src>/latest (stamped ts)
    msg.data["ts"] = nowUnix();
    String path = "/radiant/telemetry/" + msg.src + "/latest";
    char json[512];
    serializeJson(msg.data, json, sizeof(json));
    cloud.setJson(path.c_str(), json);
  } else if (msg.type == MsgType::State) {
    // Forward to Firebase: radiant/state/<src>
    String path = "/radiant/state/" + msg.src;
    char json[256];
    serializeJson(msg.data, json, sizeof(json));
    cloud.setJson(path.c_str(), json);
  } else if (msg.type == MsgType::Status && msg.src == "chiller") {
    // Peer booted after us - re-send the current pump state so it converges.
    sendPumpCmd(pumpsOn);
  }
  // Other messages (status from dh, unknown) are logged only.
}

// ---- Firebase realtime stream handler (radiant/config) ----
void onConfigStream(const char* path, const String& json) {
  Serial.printf("[config] %s -> %s\n", path, json.c_str());

  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return;

  // /radiant/config/control/params (+ children) -> update chiller control params
  if (strncmp(path, "/radiant/config/control/params", 30) == 0) {
    if (doc["comfort_setpoint_c"].is<float>())
      params.comfortSetpointC = doc["comfort_setpoint_c"];
    if (doc["dewpoint_margin_c"].is<float>())
      params.dewPointMarginC = doc["dewpoint_margin_c"];
    if (doc["weather_cool_temp_c"].is<float>())
      params.weatherCoolTempC = doc["weather_cool_temp_c"];
    Serial.printf("[config] control params: setpoint=%.1f margin=%.1f weather=%.1f\n",
                  params.comfortSetpointC, params.dewPointMarginC,
                  params.weatherCoolTempC);
  }
  // /radiant/config/weather (written by the Flutter app) -> outdoor conditions
  else if (strncmp(path, "/radiant/config/weather", 23) == 0) {
    inputs.outdoorTempC     = doc["temp_c"]     | -100.0f;
    inputs.outdoorDewPointC = doc["dewpoint_c"] | -100.0f;
    weatherValid = true;
    lastWeatherUpdateMs = millis();
    Serial.printf("[config] weather: temp=%.1f dewpoint=%.1f\n",
                  inputs.outdoorTempC, inputs.outdoorDewPointC);
  }
  // /radiant/config/dh (+ children) -> forward dehumidifier config over ESP-NOW
  else if (strncmp(path, "/radiant/config/dh", 18) == 0) {
    JsonDocument payload;
    payload["humidity_setpoint_pct"] = doc["humidity_setpoint_pct"] | 55.0f;
    payload["humidity_deadband_pct"] = doc["humidity_deadband_pct"] | 5.0f;
    sendTo(PEER_DEHUM, MsgType::Config, payload);
  }
}

void setup() {
  Serial.begin(115200);

  espNowQueue = xQueueCreate(8, sizeof(EspNowRxPacket));

  loopTemps.begin();

  // Connect Wi-Fi FIRST (so ESP-NOW + Firebase share the router channel).
  // WiFiManager opens a "RadiantCooling-AP" portal when there are no saved
  // credentials or the network is unreachable.
  if (!wifi.begin()) {
    Serial.println("WiFi not connected - connect to AP 'RadiantCooling-AP' to configure");
    ESP.restart();
  }
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // for telemetry ts

  if (!espNow.begin()) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  // IMPORTANT (WiFiManager + ESP-NOW): the gateway is now on the ROUTER's
  // Wi-Fi channel (whatever channel the router uses). The ESP-NOW peers sit
  // on channel 1 by default, so fix the router to 1/6/11 and/or configure
  // the peers' channel to match - otherwise they won't hear the gateway.
  // The gateway's channel is published in the heartbeat for reference.
  espNow.addPeer(PEER_CHILLER);
  espNow.addPeer(PEER_DEHUM);
  espNow.onReceive(handleMessage);   // enqueue only - decoded in loop()

  // Firebase (async): auth + realtime stream on the config path.
  // The stream auto-starts in cloud.loop() once sign-in completes.
  FirebaseSync::Config fbCfg = {
    FIREBASE_URL, FIREBASE_API_KEY, FIREBASE_EMAIL, FIREBASE_PASSWORD
  };
  cloud.begin(fbCfg);
  cloud.stream("/radiant/config", onConfigStream);
}

void loop() {
  // WiFi reset button: hold ~3 s to erase credentials and restart
  wifi.handleResetButton(WIFI_RESET_HOLD_MS);

  // Firebase async processing (network, auth token, stream heartbeats)
  cloud.loop();

  // Read local 6x DS18B20 sensors
  loopTemps.requestTemperatures();
  delay(750);                                  // DS18B20 conversion time

  // Sensor inputs: chilled-water supply/return + pipe temperatures from the
  // local DS18B20 array (index roles in Config.h). Coldest pipe = min of the
  // VALID readings - the true anti-condensation surface above the ceiling.
  float pipeTemps[TEMP_COUNT];
  for (uint8_t i = 0; i < loopTemps.count(); i++) pipeTemps[i] = loopTemps.readC(i);
  inputs.supplyTempC  = pipeTemps[IDX_SUPPLY];
  inputs.returnTempC  = pipeTemps[IDX_RETURN];
  inputs.coldestPipeC = ClimateControl::coldestValidC(pipeTemps, loopTemps.count());

  // Drain the ESP-NOW RX queue: peer telemetry/state/status
  EspNowRxPacket pkt;
  while (espNowQueue && xQueueReceive(espNowQueue, &pkt, 0) == pdTRUE) {
    IncomingMessage msg;
    if (JsonProtocol::decode((const char*)pkt.data, pkt.len, msg)) {
      handleIncoming(msg);
    }
  }

  // Outdoor weather arrives from the Flutter app via the config/weather
  // stream. When it goes stale (app offline), drop it: weather demand turns
  // off and the condensation floor falls back to the indoor dew point.
  if (weatherValid && millis() - lastWeatherUpdateMs >= WEATHER_STALE_S * 1000UL) {
    weatherValid = false;
    inputs.outdoorTempC     = -100.0f;
    inputs.outdoorDewPointC = -100.0f;
    Serial.println("[weather] stale (app offline) - indoor dew point only");
  }

  // Control computation (weather + sensors + condensation, see flow-chart.md)
  ControlDecision d = ClimateControl::decidePumps(inputs, params, pumpsOn);
  if (d.pumpsOn != pumpsOn) {
    pumpsOn = d.pumpsOn;
    sendPumpCmd(pumpsOn);
  }

  // ---- SAVE: telemetry + heartbeat to Firebase (non-blocking) ----
  if (millis() - lastPublishMs >= TELEMETRY_S * 1000UL) {
    lastPublishMs = millis();

    // radiant/telemetry/monitor/latest (temps_c = [supply, return, pipe1..4])
    JsonDocument tel;
    JsonArray temps = tel["temps_c"].to<JsonArray>();
    for (uint8_t i = 0; i < loopTemps.count(); i++) temps.add(loopTemps.readC(i));
    tel["supply_c"]       = inputs.supplyTempC;
    tel["return_c"]       = inputs.returnTempC;
    tel["coldest_pipe_c"] = inputs.coldestPipeC;
    tel["delta_t_c"] = (inputs.returnTempC > kTempValidLoC && inputs.returnTempC < kTempValidHiC &&
                        inputs.supplyTempC > kTempValidLoC && inputs.supplyTempC < kTempValidHiC)
                            ? inputs.returnTempC - inputs.supplyTempC : 0.0f;
    tel["dew_point_c"]    = d.refDewPointC;
    tel["water_floor_c"]  = d.waterFloorC;
    tel["pumps"]          = pumpsOn ? "on" : "off";
    tel["ts"]             = nowUnix();
    char json[512];
    serializeJson(tel, json, sizeof(json));
    cloud.setJson("/radiant/telemetry/monitor/latest", json);

    // Retained heartbeat (exposes SYSTEM_ID for app linking + Wi-Fi channel)
    JsonDocument hb;
    hb["online"]    = true;
    hb["firmware"]  = "0.1.0";
    hb["channel"]   = WiFi.channel();
    hb["device_id"] = SYSTEM_ID;
    hb["ts"]        = nowUnix();
    char hbJson[256];
    serializeJson(hb, hbJson, sizeof(hbJson));
    cloud.setJson("/radiant/heartbeat/monitor", hbJson);

    // Device registry - the app discovers/links this system via SYSTEM_ID.
    JsonDocument dev;
    dev["online"]    = true;
    dev["firmware"]  = "0.1.0";
    dev["channel"]   = WiFi.channel();
    dev["device_id"] = SYSTEM_ID;
    dev["ts"]        = nowUnix();
    char devJson[256];
    serializeJson(dev, devJson, sizeof(devJson));
    cloud.setJson(String("/radiant/devices/") + SYSTEM_ID, devJson);
  }

  delay(100);
}
