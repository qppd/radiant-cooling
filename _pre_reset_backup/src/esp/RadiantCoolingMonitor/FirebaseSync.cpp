#include "FirebaseSync.h"

FirebaseSync* FirebaseSync::_instance = nullptr;

FirebaseSync::FirebaseSync() {}

FirebaseSync::~FirebaseSync() {}

void FirebaseSync::begin(const Config& cfg) {
  _instance = this;

  _fbdo.setInsecure();
  _streamFbdo.setInsecure();

  _config.database_url = cfg.url;
  _config.api_key = cfg.apiKey;

  _config.auth.token.email = cfg.email;
  _config.auth.token.password = cfg.password;

  Firebase.begin(&_config, &_auth);
  Firebase.reconnectWiFi(true);

  Serial.println("[firebase] begin OK");
}

void FirebaseSync::loop() {
  Firebase.loop();

  if (_streamPath && !_streamStarted && ready()) {
    _streamFbdo.setStreamCallback(
      _streamCallback,
      _streamTimeoutCallback,
      1000
    );

    if (Firebase.beginStream(_streamFbdo, _streamPath)) {
      _streamStarted = true;
      Serial.printf("[firebase] stream started on %s\n", _streamPath);
    } else {
      Serial.printf("[firebase] stream start FAILED: %s\n",
                    _streamFbdo.errorReason().c_str());
    }
  }

  if (_streamStarted) {
    Firebase.readStream(&_streamFbdo);
  }
}

bool FirebaseSync::ready() {
  return Firebase.ready();
}

bool FirebaseSync::setJson(const String& path, const String& json) {
  if (!ready()) return false;
  return Firebase.setJSON(_fbdo, path.c_str(), json.c_str());
}

bool FirebaseSync::updateJson(const String& path, const String& json) {
  if (!ready()) return false;
  return Firebase.updateNode(_fbdo, path.c_str(), json.c_str());
}

bool FirebaseSync::removeNode(const String& path) {
  if (!ready()) return false;
  return Firebase.deleteNode(_fbdo, path.c_str());
}

bool FirebaseSync::stream(const char* path, StreamCb cb) {
  _streamPath = path;
  _streamCb = cb;
  _streamStarted = false;
  return true;
}


void FirebaseSync::_streamCallback(FirebaseStreamData result) {
  if (!_instance || !_instance->_streamCb) return;

  const String eventPath = result.dataPath();
  const String subPath = String(_instance->_streamPath);
  if (!eventPath.startsWith(subPath) && eventPath != subPath) {
    return;
  }

  if (result.eventType() == "put" || result.eventType() == "patch") {
    _instance->_streamCb(result.dataPath().c_str(), result.stringData());
  }
}

void FirebaseSync::_streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("[firebase] stream timeout, reconnecting...");
  }
}
