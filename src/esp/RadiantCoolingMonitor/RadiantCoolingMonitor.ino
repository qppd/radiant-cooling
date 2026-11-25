
#include "Config.h"
#include "TemperatureSensor.h"
#include "WifiProvisioner.h"
#include "EspNowTransport.h"
#include "JsonProtocol.h"
#include "FirebaseSync.h"
#include "WeatherApi.h"
#include "ClimateControl.h"
#include "SettingsStore.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
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
SettingsStore settings;

unsigned long lastPublishMs = 0;
unsigned long lastWeatherMs = 0;
uint32_t seq = 0;

ControlInputs inputs;
ControlParams  params;
DhConfig      dhConfig;
bool pumpsOn = false;
unsigned long lastPumpCmdMs = 0;
bool pumpCmdPending = true;   // force one command after boot / gateway restart
bool dhConfigDirty = true;    // resend cached DH settings on first DH contact

// Written by the cloud task and read by loop()/the serial console.
volatile bool cloudOnlineFlag = false;
bool wifiWasConnected = false;

// Weather is fetched on the cloud task and consumed by the control loop, so
// the snapshot is mutex-protected instead of racing floats across tasks.
struct SharedWeather {
  bool  valid = false;
  float tempC = -100.0f;
  float dewPointC = -100.0f;
  float humidityPct = -100.0f;
  unsigned long updatedMs = 0;
};

SharedWeather wxState;
SemaphoreHandle_t wxMutex = nullptr;

struct SharedWeather weatherGet() {
  SharedWeather copy;
  if (!wxMutex) return wxState;
  xSemaphoreTake(wxMutex, portMAX_DELAY);
  copy = wxState;
  xSemaphoreGive(wxMutex);
  return copy;
}

void weatherSet(const WeatherConditions& wx) {
  if (!wxMutex) return;
  xSemaphoreTake(wxMutex, portMAX_DELAY);
  wxState.valid = true;
  wxState.tempC = wx.tempC;
  wxState.dewPointC = wx.dewPointC;
  wxState.humidityPct = wx.humidityPct;
  wxState.updatedMs = millis();
  xSemaphoreGive(wxMutex);
}

void weatherClear() {
  if (!wxMutex) return;
  xSemaphoreTake(wxMutex, portMAX_DELAY);
  wxState.valid = false;
  wxState.tempC = -100.0f;
  wxState.dewPointC = -100.0f;
  wxState.humidityPct = -100.0f;
  wxState.updatedMs = 0;
  xSemaphoreGive(wxMutex);
}

bool weatherIsFresh() {
  if (WiFi.status() != WL_CONNECTED) return false;
  SharedWeather wx = weatherGet();
  return wx.valid && (millis() - wx.updatedMs < WEATHER_STALE_S * 1000UL);
}

// Control stream state tracking (avoid re-sending unchanged values)
String lastCtrlMode = "";
float  lastCtrlSetpoint = -999.0f;


typedef struct {
  uint8_t mac[6];
  uint8_t data[250];
  size_t  len;
} EspNowRxPacket;
QueueHandle_t espNowQueue;


// ── Firebase cloud task ───────────────────────────────────────────────────
// ALL Firebase + HTTPS-weather work runs here on a dedicated 20 KB stack. The
// synchronous TLS handshakes (Firebase RTDB writes, stream (re)connects and
// the WeatherAPI fetch) overflow the 8 KB loopTask stack - that was the
// "Stack canary watchpoint triggered (loopTask)" panic. loop() only enqueues
// path+json pairs; this task drains them (latest write per path wins) so a
// burst of peer telemetry costs one connection open per path, not per packet.
typedef struct {
  char path[96];
  char json[512];
} CloudWrite;

QueueHandle_t cloudQueue = nullptr;
static const uint8_t kCloudBatchMax = 8;
// TLS handshakes (Firebase RTDB writes, stream (re)connects, WeatherAPI) need
// far more than the 8 KB loopTask stack. This task owns them exclusively.
static const uint32_t kCloudTaskStack = 20480;

void enqueueCloudWrite(const String& path, const String& json) {
  if (!cloudQueue) return;
  CloudWrite item;
  strlcpy(item.path, path.c_str(), sizeof(item.path));
  strlcpy(item.json, json.c_str(), sizeof(item.json));
  if (xQueueSend(cloudQueue, &item, 0) != pdTRUE) {
    Serial.println("[cloud] queue full - write dropped (newest already queued)");
  }
}

volatile bool wxRequestDue = false;  // set by loop()/serial 'wx', consumed by cloudTask
unsigned long lastKeyRefreshMs = 0;  // last WeatherAPI-key pull from Firebase

// The app publishes the WeatherAPI key at radiant/config/weather_key and the
// config stream picks it up instantly - but a stream that dropped, a reboot
// before the key existed, or a key set while the gateway was offline all left
// the gateway with no key for good. So read the key back over REST too: retry
// every minute while we have none, then refresh occasionally for rotations.
void serviceWeatherKey(bool force) {
  if (!cloud.isReady()) return;

  const bool haveKey = weather.hasKey();
  const uint32_t intervalMs = (haveKey ? WEATHER_KEY_REFRESH_S : WEATHER_KEY_RETRY_S) * 1000UL;
  if (!force && millis() - lastKeyRefreshMs < intervalMs) return;
  lastKeyRefreshMs = millis();

  String key;
  if (!cloud.readString(RadiantFirebaseConfig::getConfigWeatherKeyPath(), key)) {
    if (!haveKey) {
      Serial.println("[weather] no WeatherAPI key yet - check radiant/config/weather_key "
                     "(set it in the app) and that this gateway may read it");
    }
    return;
  }
  if (strcmp(key.c_str(), weather.key()) == 0) return;

  weather.setKey(key.c_str());
  Serial.printf("[weather] API key %s from Firebase (%u chars)\n",
                haveKey ? "rotated" : "loaded", (unsigned)key.length());
}

// Publish the reading to its own node so the app has live outdoor weather
// without waiting for the 30 s monitor telemetry cycle.
void publishWeather(const WeatherConditions& wx) {
  JsonDocument doc;
  doc["location"]     = WEATHER_LOCATION;
  doc["temp_c"]       = wx.tempC;
  doc["feels_like_c"] = wx.feelsLikeC;
  doc["humidity_pct"] = wx.humidityPct;
  doc["dew_point_c"]  = wx.dewPointC;
  doc["wind_kph"]     = wx.windKph;
  doc["condition"]    = wx.condition;
  doc["is_day"]       = wx.isDay;
  doc["obs_epoch"]    = wx.obsEpoch;
  doc["ts"]           = nowUnix();  // required by the database rules
  char json[448];
  serializeJson(doc, json, sizeof(json));
  cloud.setJson(
    String(RadiantFirebaseConfig::getTelemetryBasePath()) + "/weather/latest",
    json);
}

void doWeatherFetch() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[weather] skipped - WiFi is offline (sensor-only control)");
    return;
  }

  WeatherConditions wx = weather.fetch();
  if (wx.ok) {
    weatherSet(wx);
    Serial.printf("[weather] ok: temp=%.1fC dewpoint=%.1fC\n",
                  wx.tempC, wx.dewPointC);
    publishWeather(wx);
  } else {
    Serial.println("[weather] fetch failed (key missing or network error)");
    lastKeyRefreshMs = 0;  // the key may have just been set - re-read at once
  }
}

void cloudTask(void*) {
  // Heap-allocated: kCloudBatchMax * sizeof(CloudWrite) is ~4.8 KB that must
  // not sit on the same stack the TLS handshake needs.
  CloudWrite* batch = (CloudWrite*)malloc(kCloudBatchMax * sizeof(CloudWrite));
  if (!batch) {
    Serial.println("[cloud] batch alloc failed - cloud task stopped");
    vTaskDelete(nullptr);
    return;
  }

  uint32_t retryDelayMs = CLOUD_RETRY_MS;
  uint32_t lastInitAttemptMs = 0;
  bool everAttempted = false;
  bool wifiWasUp = false;

  for (;;) {
    const bool wifiUp = WiFi.status() == WL_CONNECTED;

    if (wifiUp != wifiWasUp) {
      wifiWasUp = wifiUp;
      if (wifiUp) {
        // Link just came back: try Firebase immediately, then fetch weather.
        everAttempted = false;
        retryDelayMs = CLOUD_RETRY_MS;
        Serial.println("[cloud] WiFi restored - reconnecting Firebase");
      } else {
        Serial.println("[cloud] WiFi lost - local control continues");
      }
    }

    if (wifiUp) {
      if (!cloud.isReady() &&
          (!everAttempted || millis() - lastInitAttemptMs >= retryDelayMs)) {
        everAttempted = true;
        lastInitAttemptMs = millis();
        Serial.println("[cloud] initializing Firebase...");
        cloud.begin();
        if (cloud.isReady()) {
          retryDelayMs = CLOUD_RETRY_MS;
          Serial.println("[cloud] Firebase ready");
          wxRequestDue = true;  // re-read key + refresh weather straight away
        } else {
          retryDelayMs = min(retryDelayMs * 2UL, CLOUD_RETRY_MAX_MS);
          Serial.printf("[cloud] not ready - next attempt in %lu s\n",
                        (unsigned long)(retryDelayMs / 1000));
        }
      }

      cloud.loop();  // stream reads + reconnects + user stream callbacks
      cloudOnlineFlag = cloud.isReady();

      if (wxRequestDue) {
        wxRequestDue = false;
        serviceWeatherKey(true);  // the user asked - don't wait for the retry timer
        doWeatherFetch();
      } else {
        serviceWeatherKey(false);
      }
    } else {
      // No TLS, no Firebase and no weather work while the link is down.
      cloudOnlineFlag = false;
      if (wxRequestDue) {
        static unsigned long lastOfflineWxLogMs = 0;
        if (millis() - lastOfflineWxLogMs >= 60000UL) {
          lastOfflineWxLogMs = millis();
          Serial.println("[weather] request queued - waiting for WiFi");
        }
      }
    }

    int n = 0;
    while (n < kCloudBatchMax &&
           xQueueReceive(cloudQueue, &batch[n], 0) == pdTRUE) {
      n++;
    }
    if (n == 0) {
      // Idle: block up to 200 ms so the stream is still serviced promptly.
      if (xQueueReceive(cloudQueue, &batch[n], 200) == pdTRUE) n++;
    }

    // Write the batch, but skip entries superseded by a newer write to the
    // same path (latest wins) - bounds the number of connection opens. When
    // offline the batch is simply dropped; fresh telemetry follows on
    // reconnection.
    if (cloud.isReady()) {
      for (int i = 0; i < n; i++) {
        bool superseded = false;
        for (int j = i + 1; j < n; j++) {
          if (strcmp(batch[i].path, batch[j].path) == 0) {
            superseded = true;
            break;
          }
        }
        if (!superseded) cloud.setJson(batch[i].path, batch[i].json);
      }
    }
    if (n > 0) vTaskDelay(pdMS_TO_TICKS(50));  // pace the connection opens
  }
}

// lastCtrlMode is a heap String written from the cloud task (control stream
// callback) and read from loopTask - guard it with a mutex.
SemaphoreHandle_t modeMutex = nullptr;

String getCtrlMode() {
  if (!modeMutex) return lastCtrlMode;
  xSemaphoreTake(modeMutex, portMAX_DELAY);
  String s = lastCtrlMode;
  xSemaphoreGive(modeMutex);
  return s;
}

void setCtrlMode(const String& m) {
  if (!modeMutex) { lastCtrlMode = m; return; }
  xSemaphoreTake(modeMutex, portMAX_DELAY);
  lastCtrlMode = m;
  xSemaphoreGive(modeMutex);
}


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
  lastPumpCmdMs = millis();
  Serial.printf("[gateway] cmd chiller: set_pumps %s\n", on ? "on" : "off");
}

// Resend the cached dehumidifier settings. The DH peer keeps them in RAM, so
// this runs after either board reboots (and does not need Firebase).
void sendDhConfig() {
  JsonDocument payload;
  payload["humidity_setpoint_pct"] = dhConfig.humiditySetpointPct;
  payload["humidity_deadband_pct"] = dhConfig.humidityDeadbandPct;
  sendTo(PEER_DEHUM, MsgType::Config, payload);
  Serial.printf("[config] DH sent: setpoint=%.1f%% deadband=%.1f%%\n",
                dhConfig.humiditySetpointPct, dhConfig.humidityDeadbandPct);
}

// Adopt the chiller's reported output state so a gateway reboot does not keep
// a stale local "pumps off" view (hysteresis depends on currentlyOn). Ignore
// telemetry that may have been generated just before our own command.
void syncChillerPumps(const JsonDocument& data) {
  const char* compressor = data["compressor"] | "";
  const char* pump1      = data["pump1"] | "";
  if (compressor[0] == '\0' && pump1[0] == '\0') return;
  if (millis() - lastPumpCmdMs <= 5000UL) return;

  const bool peerOn = pump1[0] != '\0'
                          ? (strcmp(pump1, "on") == 0)
                          : (strcmp(compressor, "on") == 0);
  if (peerOn != pumpsOn) {
    pumpsOn = peerOn;
    pumpCmdPending = false;
    Serial.printf("[sync] chiller reports pumps %s\n", peerOn ? "on" : "off");
  }
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
      // Confirm the cached config once the peer is actually talking to us
      // (the copy sent right after a stream update may have been lost while
      // the DH board was still booting).
      if (dhConfigDirty) {
        sendDhConfig();
        dhConfigDirty = false;
      }
    } else if (msg.src == "chiller") {
      inputs.waterTempC = msg.data["water_temp_c"] | -100.0f;
      syncChillerPumps(msg.data);
    }

    if (cloudOnlineFlag) {
      msg.data["ts"] = nowUnix();
      String path = String(RadiantFirebaseConfig::getTelemetryBasePath()) +
                    "/" + msg.src + "/latest";
      char json[512];
      serializeJson(msg.data, json, sizeof(json));
      enqueueCloudWrite(path, json);
    }
  } else if (msg.type == MsgType::State) {
    if (msg.src == "chiller") syncChillerPumps(msg.data);

    if (cloudOnlineFlag) {
      String path = String(RadiantFirebaseConfig::getStateBasePath()) + "/" + msg.src;
      char json[256];
      serializeJson(msg.data, json, sizeof(json));
      enqueueCloudWrite(path, json);
    }
  } else if (msg.type == MsgType::Status) {
    // A peer rebooted (or just came up): restore the state it cannot know.
    if (msg.src == "chiller") sendPumpCmd(pumpsOn);
    else if (msg.src == "dh") sendDhConfig();
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
  settings.saveParams(params);

  // Handle weather key update
  if (stream.get("/weather_key")) {
    String key = stream.value;
    key.replace("\"", "");
    key.trim();
    if (key.length() > 0) {
      weather.setKey(key.c_str());
      lastKeyRefreshMs = millis();  // no need for the REST re-read now
      wxRequestDue = true;          // fetch immediately with the new key
      Serial.println("[config] weather key updated");
    }
  }

  // Handle dehumidifier config (full object or individual fields)
  bool dhChanged = false;
  if (stream.get("/dh/humidity_setpoint_pct")) {
    float v = stream.value.toFloat();
    if (v > 0.0f && v <= 100.0f) {
      dhConfig.humiditySetpointPct = v;
      dhChanged = true;
    }
  }
  if (stream.get("/dh/humidity_deadband_pct")) {
    float v = stream.value.toFloat();
    if (v >= 0.0f && v <= 30.0f) {
      dhConfig.humidityDeadbandPct = v;
      dhChanged = true;
    }
  }
  if (stream.get("/dh")) {
    // Full dh object received
    FirebaseJson json;
    FirebaseJsonData result;
    json.setJsonData(stream.value);

    float setpoint = dhConfig.humiditySetpointPct;
    float deadband = dhConfig.humidityDeadbandPct;
    if (json.get(result, "humidity_setpoint_pct")) setpoint = result.to<float>();
    if (json.get(result, "humidity_deadband_pct")) deadband = result.to<float>();

    dhConfig.humiditySetpointPct = setpoint;
    dhConfig.humidityDeadbandPct = deadband;
    dhChanged = true;
  }
  if (dhChanged) {
    dhConfigDirty = true;  // confirm on the next DH telemetry as well
    settings.saveDh(dhConfig);
    sendDhConfig();
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
      pumpCmdPending = false;
      sendPumpCmd(true);
    } else if (cmdOff && pumpsOn) {
      Serial.println("[control] Pumps OFF commanded from dashboard");
      pumpsOn = false;
      pumpCmdPending = false;
      sendPumpCmd(false);
    }
  }

  // Handle mode change (auto/manual)
  if (stream.get("/mode")) {
    String newMode = stream.value;
    newMode.replace("\"", "");
    newMode.trim();

    String currentMode = getCtrlMode();
    if (newMode != currentMode) {
      Serial.printf("[control] Mode changed: '%s' -> '%s'\n",
                    currentMode.c_str(), newMode.c_str());
      setCtrlMode(newMode);

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
  settings.saveParams(params);

  // Handle dehumidifier setpoints forwarded to DH controller
  bool dhChanged = false;
  if (stream.get("/dh/humidity_setpoint_pct")) {
    float v = stream.value.toFloat();
    if (v > 0.0f && v <= 100.0f) {
      dhConfig.humiditySetpointPct = v;
      dhChanged = true;
    }
  }
  if (stream.get("/dh/humidity_deadband_pct")) {
    float v = stream.value.toFloat();
    if (v >= 0.0f && v <= 30.0f) {
      dhConfig.humidityDeadbandPct = v;
      dhChanged = true;
    }
  }
  if (stream.get("/dh")) {
    FirebaseJson json;
    FirebaseJsonData result;
    json.setJsonData(stream.value);

    float setpoint = dhConfig.humiditySetpointPct;
    float deadband = dhConfig.humidityDeadbandPct;
    if (json.get(result, "humidity_setpoint_pct")) setpoint = result.to<float>();
    if (json.get(result, "humidity_deadband_pct")) deadband = result.to<float>();

    dhConfig.humiditySetpointPct = setpoint;
    dhConfig.humidityDeadbandPct = deadband;
    dhChanged = true;
  }
  if (dhChanged) {
    dhConfigDirty = true;  // confirm on the next DH telemetry as well
    settings.saveDh(dhConfig);
    sendDhConfig();
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
    const bool wxFresh = weatherIsFresh();
    Serial.printf("[test] wifi %s | cloud=%s | weather=%s | ssid=%s | ip=%s | rssi=%d dBm | ap_ch=%u | espnow_ch=%u | mac=%s\n",
                  wifi.connected() ? "CONNECTED" : "DISCONNECTED",
                  cloudOnlineFlag ? "READY" : "offline",
                  wxFresh ? "valid" : "none",
                  WiFi.SSID().c_str(),
                  wifi.localIP().c_str(),
                  WiFi.RSSI(),
                  (unsigned)WiFi.channel(),
                  (unsigned)espNow.channel(),
                  WiFi.macAddress().c_str());

  } else if (cmd == "wx") {
    if (!wifi.connected()) {
      Serial.println("[test] WiFi is offline - request queued until the link returns");
    }
    Serial.println("[test] fetching outdoor weather (on the cloud task)...");
    if (!weather.hasKey()) {
      Serial.println("[test] warning: no weather API key stored (set /weather_key in Firebase)");
    }
    wxRequestDue = true;  // HTTPS fetch must run on the cloud task, not here

  } else if (cmd.startsWith("mode ")) {
    String m = cmd.substring(5);
    m.trim();
    if (m == "manual" || m == "auto") {
      setCtrlMode(m);
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
      pumpCmdPending = false;
      sendPumpCmd(on);
      if (getCtrlMode() != "manual") {
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
    String mode = getCtrlMode();
    const bool wxFresh = weatherIsFresh();
    Serial.println("=== RadiantCoolingMonitor status ===");
    Serial.printf("mode=%s  pumps=%s  wifi=%s  cloud=%s  weather=%s  control=%s\n",
                  mode.length() > 0 ? mode.c_str() : "auto",
                  pumpsOn ? "on" : "off",
                  wifi.connected() ? "up" : "down",
                  cloudOnlineFlag ? "ready" : "offline",
                  wxFresh ? "valid" : "none",
                  wxFresh ? "weather+sensors" : "sensors-only");
    Serial.printf("supply=%.2f C  return=%.2f C  coldest_pipe=%.2f C  water=%.2f C\n",
                  inputs.supplyTempC, inputs.returnTempC, inputs.coldestPipeC, inputs.waterTempC);
    Serial.printf("indoor=%.2f C %.1f %%RH  outdoor=%.2f C dew=%.2f C\n",
                  inputs.indoorTempC, inputs.indoorHumidityPct,
                  inputs.outdoorTempC, inputs.outdoorDewPointC);
    Serial.printf("setpoint=%.1f C  dewpoint_margin=%.1f C  weather_cool=%.1f C\n",
                  params.comfortSetpointC, params.dewPointMarginC, params.weatherCoolTempC);
    Serial.printf("wifi=%s  ip=%s  espnow_ch=%u\n",
                  wifi.connected() ? "up" : "down",
                  wifi.localIP().c_str(),
                  (unsigned)espNow.channel());

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
  cloudQueue  = xQueueCreate(kCloudBatchMax * 2, sizeof(CloudWrite));
  modeMutex   = xSemaphoreCreateMutex();
  wxMutex     = xSemaphoreCreateMutex();

  // Load the last settings received from the app so an offline power cycle
  // keeps the tuned values instead of reverting to the compiled defaults.
  settings.begin();
  settings.load(params, dhConfig);
  Serial.printf("[settings] setpoint=%.1f margin=%.1f weather_cool=%.1f | "
                "DH %.1f%%/%.1f%%\n",
                params.comfortSetpointC, params.dewPointMarginC,
                params.weatherCoolTempC,
                dhConfig.humiditySetpointPct, dhConfig.humidityDeadbandPct);

  beginCeilings();

  // Start the local mesh first: ESP-NOW is the control path and works with no
  // router at all. begin() pins the radio when the station is not associated.
  if (!espNow.begin(ESP_NOW_CHANNEL)) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  espNow.addPeer(PEER_CHILLER);
  espNow.addPeer(PEER_DEHUM);
  espNow.onReceive(handleMessage);
  espNow.onSend(handleSendResult); // serial TX feedback for ESP-NOW tests

  // Fail-safe before any blocking provisioning step: never leave the chiller
  // running while this gateway is unreachable. loop() re-sends as needed.
  sendPumpCmd(false);

  // Best effort only. With saved credentials this returns immediately and the
  // association runs in the background; a failure never reboots the board.
  if (!wifi.begin(ESP_NOW_CHANNEL)) {
    Serial.println("[wifi] not configured/reachable - local sensor control active");
  }
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // Register stream handlers before begin()
  cloud.setConfigStreamHandler(onConfigStream);
  cloud.setControlStreamHandler(onControlStream);

  // Firebase (auth + token + stream TLS handshakes) and the WeatherAPI HTTPS
  // fetch run ONLY on the cloud task - never on this 8 KB loopTask stack.
  xTaskCreate(cloudTask, "cloud", kCloudTaskStack, nullptr, 1, nullptr);

  Serial.println("[mode] local control active (cloud/weather optional)");
  Serial.println("[test] type 'help' for the serial test console");
}

void loop() {
  handleSerialCommand(); // manual component test console (Serial.readStringUntil)

  // Re-provisioning: stop the plant before erasing credentials + rebooting.
  if (wifi.resetRequested(WIFI_RESET_HOLD_MS)) {
    Serial.println("[wifi] reset button held - pumps OFF, then re-provisioning");
    pumpsOn = false;
    sendPumpCmd(false);
    delay(500);
    wifi.restart();
  }

  // Retry the saved network once a minute while offline. This never blocks and
  // never reboots the board.
  wifi.reconnectIfLost(WIFI_RETRY_MS);

  const bool wifiUp = wifi.connected();
  if (!wifiUp) {
    // No link: the weather feed is unusable and the radio must sit on the
    // peers' channel so local control keeps working.
    if (weatherGet().valid) {
      weatherClear();
      Serial.println("[weather] WiFi lost - sensor-only control");
    }
    if (espNow.channel() != ESP_NOW_CHANNEL &&
        espNow.setChannel(ESP_NOW_CHANNEL)) {
      Serial.printf("[espnow] offline - radio pinned to channel %u\n",
                    (unsigned)ESP_NOW_CHANNEL);
    }
  } else if (!wifiWasConnected) {
    Serial.printf("[wifi] connected: \"%s\" | ip=%s | ap_ch=%u\n",
                  WiFi.SSID().c_str(), wifi.localIP().c_str(),
                  (unsigned)WiFi.channel());
    if (WiFi.channel() != ESP_NOW_CHANNEL) {
      Serial.printf("[espnow] CHANNEL MISMATCH: AP on ch%u, peers pinned to ch%u - "
                    "every ESP-NOW send will FAIL. Set the router to channel %u.\n",
                    (unsigned)WiFi.channel(), (unsigned)ESP_NOW_CHANNEL,
                    (unsigned)ESP_NOW_CHANNEL);
    } else {
      Serial.printf("[espnow] channel %u matches ESP_NOW_CHANNEL\n",
                    (unsigned)ESP_NOW_CHANNEL);
    }
    lastWeatherMs = 0;      // refresh weather as soon as the link is back
    wxRequestDue  = true;
  }
  wifiWasConnected = wifiUp;

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

  // Weather is only trusted while the link is up and the reading is fresh.
  // On Wi-Fi loss it is dropped immediately; if only the API fails, the last
  // good reading is used until WEATHER_STALE_S.
  SharedWeather wx = weatherGet();
  const bool wxFresh = wx.valid && wifiUp &&
                       (millis() - wx.updatedMs < WEATHER_STALE_S * 1000UL);
  if (wx.valid && !wxFresh) {
    weatherClear();
    wx = weatherGet();
    Serial.println("[weather] stale - sensor-only control");
  }

  inputs.weatherValid     = wxFresh;
  inputs.outdoorTempC     = wxFresh ? wx.tempC : -100.0f;
  inputs.outdoorDewPointC = wxFresh ? wx.dewPointC : -100.0f;

  if (weather.hasKey() && wifiUp &&
      (lastWeatherMs == 0 || millis() - lastWeatherMs >= WEATHER_POLL_S * 1000UL)) {
    lastWeatherMs = millis();
    wxRequestDue = true;  // HTTPS fetch runs on the cloud task (big stack)
  }

  ControlDecision d = ClimateControl::decidePumps(inputs, params, pumpsOn);

  // Only auto-control pumps when in auto mode (or no mode set yet)
  if (getCtrlMode() != "manual") {
    if (d.pumpsOn != pumpsOn) {
      pumpsOn = d.pumpsOn;
      pumpCmdPending = false;
      sendPumpCmd(pumpsOn);
    }
  }

  // Send the first command after boot, then re-send the current state every
  // minute. `set_pumps` is idempotent on the chiller; this recovers a command
  // lost during a Wi-Fi scan or a chiller reboot.
  if (pumpCmdPending) {
    pumpCmdPending = false;
    sendPumpCmd(pumpsOn);
  } else if (millis() - lastPumpCmdMs >= PUMP_CMD_RESEND_S * 1000UL) {
    sendPumpCmd(pumpsOn);
  }

  static int lastControlSource = -1;
  const int controlSource = !d.dewPointKnown ? 2 : (d.sensorOnly ? 0 : 1);
  if (controlSource != lastControlSource) {
    lastControlSource = controlSource;
    Serial.printf("[control] source=%s (wifi=%s cloud=%s)\n",
                  controlSource == 2 ? "failsafe"
                                     : (controlSource == 0 ? "sensors-only"
                                                           : "weather+sensors"),
                  wifiUp ? "up" : "down",
                  cloudOnlineFlag ? "ready" : "offline");
    if (controlSource == 2) {
      Serial.println("[control] no indoor dew point - pumps held OFF (fail-safe)");
    }
  }

  if (millis() - lastPublishMs >= TELEMETRY_S * 1000UL) {
    lastPublishMs = millis();

    // Skip cloud work while offline; fresh telemetry follows on reconnection.
    if (cloudOnlineFlag) {
      JsonDocument tel;
      JsonArray temps = tel["temps_c"].to<JsonArray>();
      for (uint8_t i = 0; i < TEMP_COUNT; i++) temps.add(pipeTemps[i]);
      tel["supply_c"]       = inputs.supplyTempC;
      tel["return_c"]       = inputs.returnTempC;
      tel["coldest_pipe_c"] = inputs.coldestPipeC;
      tel["delta_t_c"] = (inputs.returnTempC > kTempValidLoC && inputs.returnTempC < kTempValidHiC &&
                          inputs.supplyTempC > kTempValidLoC && inputs.supplyTempC < kTempValidHiC)
                              ? inputs.returnTempC - inputs.supplyTempC : 0.0f;
      tel["outdoor_temp_c"]       = inputs.outdoorTempC;
      tel["outdoor_dewpoint_c"]   = inputs.outdoorDewPointC;
      tel["outdoor_humidity_pct"] = wxFresh ? wx.humidityPct : -100.0f;
      tel["weather_valid"]        = wxFresh;
      tel["control_source"] = !d.dewPointKnown ? "failsafe"
                            : (d.sensorOnly ? "sensors_only" : "weather+sensors");
      tel["dew_point_c"]    = d.refDewPointC;
      tel["water_floor_c"]  = d.waterFloorC;
      tel["pumps"]          = pumpsOn ? "on" : "off";
      String mode = getCtrlMode();
      tel["mode"]           = mode.length() > 0 ? mode : "auto";
      tel["ts"]             = nowUnix();
      char json[512];
      serializeJson(tel, json, sizeof(json));
      enqueueCloudWrite(
        String(RadiantFirebaseConfig::getTelemetryBasePath()) + "/monitor/latest",
        json);

      // Heartbeat and device-registry entries carry the same payload - build it
      // once and let the cloud task write both paths in one batch.
      JsonDocument hb;
      hb["online"]    = true;
      hb["firmware"]  = "0.1.0";
      hb["channel"]   = WiFi.channel();
      hb["device_id"] = SYSTEM_ID;
      hb["ts"]        = nowUnix();
      char hbJson[256];
      serializeJson(hb, hbJson, sizeof(hbJson));
      enqueueCloudWrite(RadiantFirebaseConfig::getHeartbeatPath(), hbJson);
      enqueueCloudWrite(
        String(RadiantFirebaseConfig::getDevicesBasePath()) + "/" + SYSTEM_ID,
        hbJson);
    }
  }

  delay(100);
}
