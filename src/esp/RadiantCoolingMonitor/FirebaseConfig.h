/*
 * FirebaseConfig.h - Firebase credentials + database path layout (gateway)
 *
 * Centralizes the gateway's Firebase Realtime Database configuration:
 * endpoints, credentials, and the database path layout. The class
 * declaration lives here (committed); the REAL values live in
 * FirebaseConfig.cpp, which is git-ignored - copy
 * FirebaseConfig.cpp.example to FirebaseConfig.cpp and fill it in.
 *
 * Everything is a static getter so modules (FirebaseSync, the sketch glue)
 * read from one place and no URL/path is hardcoded elsewhere.
 */
#pragma once
#include <Arduino.h>

class RadiantFirebaseConfig {
public:
  // ---- Endpoints / credentials (values in FirebaseConfig.cpp) ----
  // Firebase host for the RTDB REST endpoint (no scheme).
  static const char* getFirebaseHost();

  // Full database URL, e.g. "https://radiant-cooling-default-rtdb.firebaseio.com/".
  static const char* getDatabaseURL();

  // Web API key (Firebase console -> Project settings -> General).
  static const char* getApiKey();

  // Firebase project id, e.g. "radiant-cooling".
  static const char* getProjectId();

  // Dedicated gateway account - docs/firebase-security-rules.json identifies
  // the gateway by auth.token.email, so this MUST match the rules' email.
  static const char* getAuthEmail();
  static const char* getAuthPassword();   // email/password auth

  // ---- Realtime Database path layout (see docs/api.md) ----
  static const char* getConfigPath();              // "/radiant/config" - streamed by the gateway
  static const char* getConfigControlParamsPath(); // "/radiant/config/control/params"
  static const char* getConfigDhPath();            // "/radiant/config/dh"
  static const char* getConfigWeatherKeyPath();    // "/radiant/config/weather_key"
  static const char* getTelemetryBasePath();       // "/radiant/telemetry"  (+ "/<device>/latest")
  static const char* getStateBasePath();           // "/radiant/state"      (+ "/<device>")
  static const char* getHeartbeatPath();           // "/radiant/heartbeat/monitor"
  static const char* getDevicesBasePath();         // "/radiant/devices"    (+ "/<SYSTEM_ID>")
};
