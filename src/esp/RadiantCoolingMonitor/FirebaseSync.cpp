#include "FirebaseSync.h"

FirebaseSync* FirebaseSync::_instance = nullptr;

FirebaseSync::FirebaseSync() : _aClient(_sslClient), _streamClient(_streamSslClient) {}

FirebaseSync::~FirebaseSync() {}

void FirebaseSync::begin(const Config& cfg) {
  _instance = this;

  UserAuth userAuth(cfg.apiKey, cfg.email, cfg.password, 3600);

  _sslClient.setInsecure();
  _sslClient.setConnectionTimeout(1000);
  _sslClient.setHandshakeTimeout(5);
  _streamSslClient.setInsecure();
  _streamSslClient.setConnectionTimeout(1000);
  _streamSslClient.setHandshakeTimeout(5);

  initializeApp(_aClient, _app, getAuth(userAuth));
  _app.getApp<RealtimeDatabase>(_rtdb);
  _rtdb.url(cfg.url);
}

void FirebaseSync::loop() {
  _app.loop();

  if (_streamPath && !_streamStarted) {
    if (_app.ready()) {
      _rtdb.get(_streamClient, _streamPath, _streamEventCb, true , "streamTask");
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
  _rtdb.set<String>(_aClient, path, json);
  return true;
}

bool FirebaseSync::updateJson(const String& path, const String& json) {
  if (!ready()) return false;
  _rtdb.update<String>(_aClient, path, json);
  return true;
}

bool FirebaseSync::removeNode(const String& path) {
  if (!ready()) return false;
  _rtdb.remove(_aClient, path);
  return true;
}

bool FirebaseSync::stream(const char* path, StreamCb cb) {
  _streamPath   = path;
  _streamCb     = cb;
  _streamStarted = false;
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
