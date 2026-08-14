/*
 * FirebaseSync.h - cloud module
 *
 * Wraps the Mobizt FirebaseClient library (async) for Firebase Realtime
 * Database on the gateway board. Works in BOTH directions:
 *   - SAVE:    setJson() / updateJson() / removeNode() - write (set),
 *              partial-update (patch) and delete (remove) nodes
 *   - RECEIVE: stream() - realtime (SSE) listener on a path (radiant/config)
 *
 * All operations are non-blocking: writes are queued, and loop() must be
 * called every main-loop cycle so the async client keeps processing
 * (network, auth token, stream events). The stream is auto-started by
 * loop() once the (async) auth sign-in completes; the library reconnects
 * it automatically if the connection drops.
 *
 * NOTE: the old Firebase-ESP-Client library is deprecated - this module
 * targets the FirebaseClient library from the Arduino Library Manager
 * (2.2.x: no DefaultNetwork, AsyncClientClass takes an SSL client directly,
 * and streaming is Database.get(..., sse = true)).
 */
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// The FirebaseClient library is modular: these defines enable the services
// used here (auth + Realtime Database) and must come before the include.
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>

class FirebaseSync {
public:
  struct Config {
    const char* url      = nullptr;   // e.g. "https://<project>-default-rtdb.firebaseio.com/"
    const char* apiKey   = nullptr;   // Web API key
    const char* email    = nullptr;   // gateway account email (rules identify
                                      // the gateway by auth.token.email)
    const char* password = nullptr;   // email/password auth

    // NOTE: docs/firebase-security-rules.json identifies the gateway by
    // auth.token.email, so the gateway must use email/password (never
    // anonymous) when those rules are deployed.
  };

  // Stream event callback: path (changed node) + json (new value).
  typedef void (*StreamCb)(const char* path, const String& json);

  FirebaseSync();
  ~FirebaseSync();

  void begin(const Config& cfg);            // init SSL clients, app, auth, database
  void loop();                              // background async processing
  bool ready();                             // app authenticated
  bool connected();                         // app authenticated

  // --- SAVE (send to Firebase) ---
  bool setJson(const String& path, const String& json);     // overwrite node (PUT)
  bool updateJson(const String& path, const String& json);  // partial update (PATCH)
  bool removeNode(const String& path);                      // delete node (DELETE)

  // --- RECEIVE (realtime stream from Firebase) ---
  // Remembers the path/callback; loop() starts the SSE stream when ready.
  bool stream(const char* path, StreamCb cb);

private:
  // The library's stream callback is a plain function pointer (no context),
  // so events are bridged through this singleton - the gateway creates a
  // single FirebaseSync, so a static instance pointer is safe here.
  static FirebaseSync* _instance;
  static void _streamEventCb(AsyncResult& aResult);

  WiFiClientSecure _sslClient;         // regular requests
  WiFiClientSecure _streamSslClient;   // dedicated client for the SSE stream
  AsyncClientClass _aClient;           // bound to _sslClient
  AsyncClientClass _streamClient;      // bound to _streamSslClient
  firebase_ns::FirebaseApp _app;
  RealtimeDatabase _rtdb;

  const char* _streamPath = nullptr;
  StreamCb _streamCb = nullptr;
  bool _streamStarted = false;
};
