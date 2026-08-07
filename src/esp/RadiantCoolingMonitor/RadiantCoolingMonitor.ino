/*
 * RadiantCoolingMonitor.ino
 *
 * ESP32 GATEWAY - reads 6x DS18B20 temperature sensors, receives ESP-NOW
 * packets from the chiller and dehumidifier boards, fetches weather data,
 * and syncs all sensor data to Firebase Realtime Database. Also computes
 * the water chiller pump control (weather + sensors + condensation).
 *
 * This file is glue only. All component/library code is encapsulated:
 *   Config.h            - board configuration (MACs, credentials, constants); includes PINS_CONFIG.h
 *   PINS_CONFIG.h       - pin assignments (1-Wire)
 *   TemperatureSensor   - wraps OneWire + DallasTemperature (6x DS18B20)
 *   EspNowTransport     - wraps WiFi + esp_now (register peers, send/receive)
 *   JsonProtocol        - wraps ArduinoJson (ESP-NOW message envelope)
 *   FirebaseSync        - wraps FirebaseClient (Realtime Database)
 *   WeatherApi          - wraps HTTPClient (WeatherAPI.com current weather)
 *   ClimateControl      - dew point + pump decision (control computation)
 *
 * See docs/ for architecture, API and control logic.
 */

#include "Config.h"
#include "TemperatureSensor.h"
#include "EspNowTransport.h"
#include "JsonProtocol.h"
#include "FirebaseSync.h"
#include "WeatherApi.h"
#include "ClimateControl.h"

// ---- Components ----
TemperatureSensor loopTemps(PIN_ONE_WIRE, TEMP_COUNT);
EspNowTransport espNow;
FirebaseSync cloud(FIREBASE_URL, FIREBASE_SECRET);
WeatherApi weather(WEATHER_API_KEY, WEATHER_LOCATION);

// ---- Telemetry state ----
unsigned long lastPublishMs = 0;
unsigned long lastFbPollMs  = 0;
unsigned long lastWeatherMs = 0;
uint32_t seq = 0;

// ---- Control state ----
WeatherConditions wx;        // latest weather fetch
ControlInputs inputs;        // latest values from all sources
ControlParams  params;       // defaults; updated from Firebase config
bool pumpsOn = false;

// ESP-NOW receive -> Firebase write is decoupled via a FreeRTOS queue:
//   QueueHandle_t espNowQueue;   // created in setup(), drained in loop()
// The ESP-NOW receive callback must ONLY enqueue (never block / call
// Firebase inside the callback).

void setup() {
  Serial.begin(115200);

  loopTemps.begin();

  // IMPORTANT: connect to Wi-Fi BEFORE espNow.begin() so ESP-NOW runs on
  // the router's channel, then lock ESP-NOW + Wi-Fi to that channel
  // (1/6/11) - either fix the router channel or call WiFi.setChannel().
  // TODO: WiFi.mode(WIFI_STA); connect to WIFI_SSID / WIFI_PASS

  if (!espNow.begin()) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  espNow.addPeer(PEER_CHILLER);
  espNow.addPeer(PEER_DEHUM);
  // TODO: espNow.onReceive(handleMessage);  // enqueue, do not block

  cloud.begin();

  // TODO: publish retained heartbeat + status to Firebase (radiant/heartbeat/monitor)
}

void loop() {
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

  // TODO: drain espNowQueue -> decode with JsonProtocol ->
  //       cloud.setJson("radiant/telemetry/<src>/latest", ...)
  // TODO: publish own telemetry to radiant/telemetry/monitor/latest:
  //       { "temps_c": [...], "dew_point_c": d.refDewPointC,
  //         "water_floor_c": d.waterFloorC, "pumps": ..., "ts": ... }
  // TODO: at FB_POLL_S interval, poll radiant/config/* with cloud.getString()
  //       and update params / forward changes to the right peer
  // TODO: cloud.loop(); reconnect Wi-Fi / Firebase if dropped
  delay(100);
}
