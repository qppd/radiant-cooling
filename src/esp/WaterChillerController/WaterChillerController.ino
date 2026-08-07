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

// ---- Components ----
TemperatureSensor waterTemp(PIN_ONE_WIRE, TEMP_COUNT);
SsrOutput pump1(PIN_SSR_PUMP1);
SsrOutput pump2(PIN_SSR_PUMP2);
EspNowTransport espNow;

// ---- Telemetry state ----
unsigned long lastSendMs = 0;
uint32_t seq = 0;

void setup() {
  Serial.begin(115200);

  waterTemp.begin();
  pump1.begin();
  pump2.begin();

  if (!espNow.begin()) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  espNow.addPeer(GATEWAY_MAC);
  // TODO: espNow.onReceive(handleMessage);  // parse cmd: set_pumps
  // TODO: espNow.onSend(handleSendResult);  // delivery status

  // TODO: send initial status message to the gateway
}

// Execute the gateway's pump command.
void setPumps(bool on) {
  pump1.set(on);
  pump2.set(on);
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

  // Periodic telemetry to gateway (see docs/api.md)
  if (millis() - lastSendMs >= TELEMETRY_S * 1000UL) {
    lastSendMs = millis();
    // TODO: build JsonDocument payload { "water_temp_c": tempC,
    //       "pump1": on/off, "pump2": on/off }
    // TODO: JsonProtocol::encode(MsgType::Telemetry, DEVICE_ID, ++seq, payload,
    //       buffer, sizeof(buffer)) -> espNow.sendTo(GATEWAY_MAC, ...)
  }

  // TODO: check espNow receive flag -> JsonProtocol::decode ->
  //       if cmd == "set_pumps": setPumps(value == "on")
  delay(100);
}
