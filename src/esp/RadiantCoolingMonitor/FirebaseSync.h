/*
 * FirebaseSync.h - cloud module
 *
 * Wraps the Mobizt FirebaseClient library (async) for Firebase Realtime
 * Database on the gateway board. Works in BOTH directions:
 *   - SAVE:    setJson() / updateJson() - write telemetry/state to Firebase
 *   - RECEIVE: stream() - realtime listener on a path (e.g. radiant/config)
 *
 * All operations are non-blocking: writes are queued when ready(), and
 * loop() must be called every main-loop cycle so the async client keeps
 * processing (network, auth token, stream heartbeats). The stream is
 * auto-started by loop() once the (async) auth sign-in completes, and it
 * re-arms itself if the connection drops.
 *
 * NOTE: the old Firebase-ESP-Client library is deprecated - this module
 * targets the FirebaseClient library from the Arduino Library Manager.
 */
#pragma once
#include <Arduino.h>
#include <FirebaseClient.h>

class FirebaseSync {
public:
  struct Config {
    const char* url      = nullptr;   // e.g. "https://<project>-default-rtdb.firebaseio.com/"
    const char* apiKey   = nullptr;   // Web API key
    const char* email    = nullptr;   // "" or nullptr = anonymous sign-in (but see rules note below)
    const char* password = nullptr;   // email/password auth

    // NOTE: docs/firebase-security-rules.json identifies the gateway by
    // auth.token.email, so the gateway should use email/password (not
    // anonymous) when those rules are deployed.
  };

  // Stream event callback: path (changed node) + json (new value).
  typedef void (*StreamCb)(const char* path, const String& json);

  FirebaseSync();
  ~FirebaseSync();

  void begin(const Config& cfg);            // init network, app, auth, database
  void loop();                              // background async processing
  bool ready() const;                       // app ready AND client not busy
  bool connected() const;                   // app signed in

  // --- SAVE (send to Firebase) ---
  bool setJson(const char* path, const String& json);     // overwrite node
  bool updateJson(const char* path, const String& json);  // partial update

  // --- RECEIVE (realtime stream from Firebase) ---
  // Remembers the path/callback; loop() starts the stream when ready.
  bool stream(const char* path, StreamCb cb);

private:
  static void _streamEventCb(firebase::AsyncResult& aResult);
  static void _streamTimeoutCb(bool& timeout);

  firebase::DefaultNetwork _network;
  firebase::UserAuth _userAuth;
  firebase::FirebaseApp _app;
  firebase::AsyncClientClass _aClient;
  firebase::RealtimeDatabase _rtdb;

  const char* _streamPath = nullptr;
  StreamCb _streamCb = nullptr;
  bool _streamActive = false;
};
