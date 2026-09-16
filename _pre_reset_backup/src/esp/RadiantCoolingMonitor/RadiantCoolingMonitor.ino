
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

TemperatureSensor loopTemps(PIN_ONE_WIRE, TEMP_COUNT);
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

void onConfigStream(const char* path, const String& json) {
  Serial.printf("[config] %s -> %s\n", path, json.c_str());

  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return;

  if (String(path).startsWith(RadiantFirebaseConfig::getConfigControlParamsPath())) {
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
  else if (String(path).startsWith(RadiantFirebaseConfig::getConfigWeatherKeyPath())) {
    const char* key = doc.as<const char*>();
    if (key != nullptr && key[0] != '\0') {
      weather.setKey(key);
      Serial.println("[config] weather key updated");
    }
  }
  else if (String(path).startsWith(RadiantFirebaseConfig::getConfigDhPath())) {
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

  FirebaseSync::Config fbCfg = {
    RadiantFirebaseConfig::getDatabaseURL(),
    RadiantFirebaseConfig::getApiKey(),
    RadiantFirebaseConfig::getAuthEmail(),
    RadiantFirebaseConfig::getAuthPassword(),
  };
  cloud.begin(fbCfg);
  cloud.stream(RadiantFirebaseConfig::getConfigPath(), onConfigStream);
}

void loop() {
  wifi.handleResetButton(WIFI_RESET_HOLD_MS);
  wifi.reconnectIfLost();

  cloud.loop();

  loopTemps.requestTemperatures();
  delay(750);

  float pipeTemps[TEMP_COUNT];
  for (uint8_t i = 0; i < loopTemps.count(); i++) pipeTemps[i] = loopTemps.readC(i);
  inputs.supplyTempC  = pipeTemps[IDX_SUPPLY];
  inputs.returnTempC  = pipeTemps[IDX_RETURN];
  inputs.coldestPipeC = ClimateControl::coldestValidC(pipeTemps, loopTemps.count());

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
  if (d.pumpsOn != pumpsOn) {
    pumpsOn = d.pumpsOn;
    sendPumpCmd(pumpsOn);
  }

  if (millis() - lastPublishMs >= TELEMETRY_S * 1000UL) {
    lastPublishMs = millis();

    JsonDocument tel;
    JsonArray temps = tel["temps_c"].to<JsonArray>();
    for (uint8_t i = 0; i < loopTemps.count(); i++) temps.add(loopTemps.readC(i));
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
