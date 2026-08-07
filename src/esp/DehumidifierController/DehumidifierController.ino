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

// ---- Components ----
HumiditySensor roomClimate(PIN_DHT22);
SsrOutput dehumidifier(PIN_SSR_DEHUM);
EspNowTransport espNow;

// ---- Runtime config (defaults; updated via ESP-NOW config messages) ----
float HUMIDITY_SETPOINT_PCT = 55.0;   // target relative humidity (%)
float HUMIDITY_DEADBAND_PCT = 5.0;    // hysteresis (%)

// ---- Telemetry state ----
unsigned long lastSendMs = 0;
uint32_t seq = 0;

void setup() {
  Serial.begin(115200);

  roomClimate.begin();
  dehumidifier.begin();

  if (!espNow.begin()) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  espNow.addPeer(GATEWAY_MAC);
  // TODO: espNow.onReceive(handleMessage);  // parse cmd/config
  // TODO: espNow.onSend(handleSendResult);  // delivery status

  // TODO: send initial status message to the gateway
}

void loop() {
  float tempC, humidityPct;

  // Humidity control with hysteresis (see docs/diagrams/flow-chart.md)
  if (roomClimate.read(tempC, humidityPct)) {
    if (humidityPct > HUMIDITY_SETPOINT_PCT + HUMIDITY_DEADBAND_PCT) {
      dehumidifier.on();
    } else if (humidityPct < HUMIDITY_SETPOINT_PCT - HUMIDITY_DEADBAND_PCT) {
      dehumidifier.off();
    }
    // Between the two thresholds: keep the current state (hysteresis)
  } else {
    // TODO: fail-safe (sensor read failure) - keep dehumidifier state?
  }

  // Periodic telemetry to gateway (see docs/api.md)
  if (millis() - lastSendMs >= TELEMETRY_S * 1000UL) {
    lastSendMs = millis();
    // TODO: build JsonDocument payload { "temp_c": tempC,
    //       "humidity_pct": humidityPct, "dehumidifier": on/off }
    // TODO: JsonProtocol::encode(MsgType::Telemetry, DEVICE_ID, ++seq, payload,
    //       buffer, sizeof(buffer)) -> espNow.sendTo(GATEWAY_MAC, ...)
  }

  // TODO: check espNow receive flag + apply config / commands
  delay(100);
}
