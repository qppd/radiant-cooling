/*
 * RadiantCoolingMonitor.ino
 *
 * ESP32 GATEWAY - reads 6x DS18B20 temperature sensors, receives ESP-NOW
 * packets from the chiller and dehumidifier boards, fetches weather data,
 * and syncs all sensor data to Firebase Realtime Database. Also computes
 * the water chiller pump control (weather + sensors + condensation).
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
 *   WeatherApi          - wraps HTTPClient (WeatherAPI.com current weather)
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
#include "WeatherApi.h"
#include "ClimateControl.h"

// ---- Components ----
TemperatureSensor loopTemps(PIN_ONE_WIRE, TEMP_COUNT);
WifiProvisioner wifi(WIFI_AP_NAME, PIN_WIFI_RESET_BUTTON);
EspNowTransport espNow;
FirebaseSync cloud;
WeatherApi weather(WEATHER_API_KEY, WEATHER_LOCATION);

// ---- Telemetry state ----
unsigned long lastPublishMs = 0;
unsigned long lastWeatherMs = 0;
uint32_t seq = 0;

// ---- Control state ----
WeatherConditions wx;        // latest weather fetch
ControlInputs inputs;        // latest values from all sources
ControlParams  params;       // defaults; updated from the config stream
bool pumpsOn = false;

// ESP-NOW receive -> Firebase write is decoupled via a FreeRTOS queue:
//   QueueHandle_t espNowQueue;   // created in setup(), drained in loop()
// The ESP-NOW receive callback must ONLY enqueue (never block / call
// Firebase inside the callback).

// ---- Firebase realtime stream handler (radiant/config) ----
void onConfigStream(const char* path, const String& json) {
  Serial.printf("[config] %s -> %s\n", path, json.c_str());

  // TODO: apply per path (see docs/api.md - radiant/config tree):
  //   "/radiant/config/control/params" -> update ControlParams
  //       { comfort_setpoint_c, dewpoint_margin_c, weather_cool_temp_c }
  //   "/radiant/config/dh"             -> forward set_humidity_target to
  //       the dehumidifier over ESP-NOW
}

void setup() {
  Serial.begin(115200);

  loopTemps.begin();

  // Connect Wi-Fi FIRST (so ESP-NOW + Firebase share the router channel).
  // WiFiManager opens a "RadiantCooling-AP" portal when there are no saved
  // credentials or the network is unreachable.
  if (!wifi.begin()) {
    Serial.println("WiFi not connected - connect to AP 'RadiantCooling-AP' to configure");
    ESP.restart();
  }

  if (!espNow.begin()) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  // IMPORTANT (WiFiManager + ESP-NOW): the gateway is now on the ROUTER's
  // Wi-Fi channel (whatever channel the router uses). The ESP-NOW peers sit
  // on channel 1 by default, so fix the router to 1/6/11 and/or configure
  // the peers' channel to match - otherwise they won't hear the gateway.
  // TODO: publish this channel to Firebase (heartbeat) so peers can sync.
  espNow.addPeer(PEER_CHILLER);
  espNow.addPeer(PEER_DEHUM);
  // TODO: espNow.onReceive(handleMessage);  // enqueue, do not block

  // Firebase (async): auth + realtime stream on the config path.
  // The stream auto-starts in cloud.loop() once sign-in completes.
  FirebaseSync::Config fbCfg = {
    FIREBASE_URL, FIREBASE_API_KEY, FIREBASE_EMAIL, FIREBASE_PASSWORD
  };
  cloud.begin(fbCfg);
  cloud.stream("/radiant/config", onConfigStream);

  // TODO: publish retained heartbeat + status to Firebase (radiant/heartbeat/monitor)
}

void loop() {
  // WiFi reset button: hold ~3 s to erase credentials and restart
  wifi.handleResetButton(WIFI_RESET_HOLD_MS);

  // Firebase async processing (network, auth token, stream heartbeats)
  cloud.loop();

  // Read local 6x DS18B20 sensors
  loopTemps.requestTemperatures();
  delay(750);                                  // DS18B20 conversion time

  // Sensor inputs: hottest room temp from the local DS18B20 array.
  // (TODO: only use the room-sensor subset once placements are known.)
  inputs.hottestRoomC = -100.0f;
  for (uint8_t i = 0; i < loopTemps.count(); i++) {
    float t = loopTemps.readC(i);
    if (t > inputs.hottestRoomC) inputs.hottestRoomC = t;
  }
  // TODO: inputs.indoorTempC / indoorHumidityPct <- latest dh telemetry
  // TODO: inputs.waterTempC                      <- latest chiller telemetry

  // Weather refresh (throttled - see WEATHER_POLL_S)
  if (millis() - lastWeatherMs >= WEATHER_POLL_S * 1000UL) {
    lastWeatherMs = millis();
    wx = weather.fetch();
    inputs.outdoorTempC     = wx.tempC;
    inputs.outdoorDewPointC = wx.dewPointC;
  }

  // Control computation (weather + sensors + condensation, see flow-chart.md)
  ControlDecision d = ClimateControl::decidePumps(inputs, params, pumpsOn);
  if (d.pumpsOn != pumpsOn) {
    pumpsOn = d.pumpsOn;
    // TODO: send cmd to chiller over ESP-NOW:
    //       { "cmd": "set_pumps", "value": pumpsOn ? "on" : "off" }
  }

  // ---- SAVE: telemetry to Firebase (non-blocking) ----
  if (millis() - lastPublishMs >= TELEMETRY_S * 1000UL) {
    lastPublishMs = millis();
    // TODO: build telemetry JSON (see docs/api.md - radiant/telemetry/monitor/latest):
    //   { "temps_c": [...], "dew_point_c": d.refDewPointC,
    //     "water_floor_c": d.waterFloorC, "pumps": ..., "ts": ... }
    // cloud.setJson("/radiant/telemetry/monitor/latest", telemetryJson);
  }

  // TODO: drain espNowQueue -> decode with JsonProtocol ->
  //       cloud.setJson("/radiant/telemetry/<src>/latest", json);
  // NOTE: the config stream re-arms automatically inside cloud.loop().
  delay(100);
}
