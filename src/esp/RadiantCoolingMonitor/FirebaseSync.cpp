#include "FirebaseSync.h"

using namespace firebase;

FirebaseSync::FirebaseSync() : _aClient(_network) {}

FirebaseSync::~FirebaseSync() {}

void FirebaseSync::begin(const Config& cfg) {
  // Auth: email/password when provided, otherwise anonymous sign-in.
  // (Enable "Anonymous" or "Email/Password" in Firebase console -> Auth.)
  if (cfg.email && cfg.email[0] != '\0') {
    _userAuth = UserAuth(cfg.apiKey, cfg.email, cfg.password, 3600);
  } else {
    _userAuth = UserAuth(cfg.apiKey);
  }

  initializeApp(_aClient, _app, getAuth(_userAuth), _aClient.loop);
  _app.getApp<RealtimeDatabase>(_rtdb);
  _rtdb.url(cfg.url);
}

void FirebaseSync::loop() {
  _network.loop();          // process network events
  _app.loop();              // auth token / app tasks
  _aClient.loop();          // pending requests + stream heartbeats

  // Auto-(re)start the stream: auth sign-in is async, so the app may not be
  // ready right after begin(). Re-arm when the connection drops.
  if (_streamPath) {
    if (_app.ready()) {
      if (!_streamActive) {
        _rtdb.stream(_aClient, _streamPath, _streamEventCb, _streamTimeoutCb);
        _streamActive = true;
      }
    } else {
      _streamActive = false;
    }
  }
}

bool FirebaseSync::ready() const {
  return _app.ready() && !_aClient.isBusy();
}

bool FirebaseSync::connected() const {
  return _app.ready();
}

bool FirebaseSync::setJson(const char* path, const String& json) {
  if (!ready()) return false;
  _rtdb.set<String>(_aClient, path, json);   // overwrite/create the node
  return true;
}

bool FirebaseSync::updateJson(const char* path, const String& json) {
  if (!ready()) return false;
  _rtdb.update<String>(_aClient, path, json); // partial multi-field update
  return true;
}

bool FirebaseSync::stream(const char* path, StreamCb cb) {
  _streamPath   = path;
  _streamCb     = cb;
  _streamActive = false;                     // loop() starts it when ready
  return true;
}

void FirebaseSync::_streamEventCb(firebase::AsyncResult& aResult) {
  // Real-time event: a node under the streamed path changed.
  if (aResult.isStream() && aResult.isData() && _streamCb) {
    _streamCb(aResult.dataPath().c_str(), aResult.to<String>());
  }
}

void FirebaseSync::_streamTimeoutCb(bool& timeout) {
  // Stream heartbeat timeout - the library reconnects automatically.
}
