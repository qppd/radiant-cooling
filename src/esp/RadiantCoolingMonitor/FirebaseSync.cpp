#include "FirebaseSync.h"

FirebaseSync* FirebaseSync::_instance = nullptr;

FirebaseSync::FirebaseSync() : _aClient(_sslClient), _streamClient(_streamSslClient) {}

FirebaseSync::~FirebaseSync() {}

void FirebaseSync::begin(const Config& cfg) {
  _instance = this;

  // Email/password is required: the deployed security rules identify the
  // gateway by auth.token.email (anonymous sign-in would be rejected).
  // UserAuth is local - initializeApp() copies the auth data into the app.
  UserAuth userAuth(cfg.apiKey, cfg.email, cfg.password, 3600);

  // ESP32 core 3.x SSL options (per the library's official examples):
  // skip certificate verification and set sane timeouts.
  _sslClient.setInsecure();
  _sslClient.setConnectionTimeout(1000);
  _sslClient.setHandshakeTimeout(5);
  _streamSslClient.setInsecure();
  _streamSslClient.setConnectionTimeout(1000);
  _streamSslClient.setHandshakeTimeout(5);

  // Async auth: sign-in completes in loop() via _app.loop().
  initializeApp(_aClient, _app, getAuth(userAuth));
  _app.getApp<RealtimeDatabase>(_rtdb);
  _rtdb.url(cfg.url);
}

void FirebaseSync::loop() {
  _app.loop();   // auth token + async task processing

  // Start the SSE stream once the app is authenticated. The library
  // reconnects the stream automatically (auth/network drops), so the
  // stream is started exactly once per stream() call.
  if (_streamPath && !_streamStarted) {
    if (_app.ready()) {
      _rtdb.get(_streamClient, _streamPath, _streamEventCb, true /* SSE */, "streamTask");
      _streamStarted = true;
    }
  }
}

bool FirebaseSync::ready() {
  return _app.ready();
}

bool FirebaseSync::connected() {
  return _app.ready();
}

bool FirebaseSync::setJson(const String& path, const String& json) {
  if (!ready()) return false;
  _rtdb.set<String>(_aClient, path, json);   // overwrite/create the node
  return true;
}

bool FirebaseSync::updateJson(const String& path, const String& json) {
  if (!ready()) return false;
  _rtdb.update<String>(_aClient, path, json); // partial multi-field update
  return true;
}

bool FirebaseSync::removeNode(const String& path) {
  if (!ready()) return false;
  _rtdb.remove(_aClient, path);               // delete the node (non-blocking)
  return true;
}

bool FirebaseSync::stream(const char* path, StreamCb cb) {
  _streamPath   = path;
  _streamCb     = cb;
  _streamStarted = false;                    // loop() (re)starts it when ready
  return true;
}

void FirebaseSync::_streamEventCb(AsyncResult& aResult) {
  if (!aResult.isResult()) return;

  if (aResult.available()) {
    RealtimeDatabaseResult& dbResult = aResult.to<RealtimeDatabaseResult>();
    if (dbResult.isStream() && _instance && _instance->_streamCb) {
      _instance->_streamCb(dbResult.dataPath().c_str(), dbResult.to<String>());
    }
  }
}
