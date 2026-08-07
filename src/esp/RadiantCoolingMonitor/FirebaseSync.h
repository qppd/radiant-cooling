/*
 * FirebaseSync.h - cloud module
 *
 * Wraps the FirebaseClient library (Mobizt) for the Firebase Realtime
 * Database. Gateway-only: writes telemetry/state, polls config, heartbeat.
 *
 * NOTE: the old Firebase-ESP-Client library is deprecated - use the
 * FirebaseClient library from the Arduino Library Manager.
 */
#pragma once
#include <Arduino.h>
#include <stdint.h>

class FirebaseSync {
public:
  FirebaseSync(const char* url, const char* secret);

  void begin();                              // Wi-Fi + Firebase auth setup
  void loop();                               // keep async client alive

  bool setJson(const char* path, const char* json);   // write a node
  bool getString(const char* path, String& out);      // read a node
  bool connected() const;

private:
  const char* _url;
  const char* _secret;
};
