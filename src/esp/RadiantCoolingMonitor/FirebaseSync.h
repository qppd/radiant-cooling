#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#include <FirebaseClient.h>

class FirebaseSync {
public:
  struct Config {
    const char* url      = nullptr;
    const char* apiKey   = nullptr;
    const char* email    = nullptr;
    const char* password = nullptr;

  };

  typedef void (*StreamCb)(const char* path, const String& json);

  FirebaseSync();
  ~FirebaseSync();

  void begin(const Config& cfg);
  void loop();
  bool ready();
  bool connected();

  bool setJson(const String& path, const String& json);
  bool updateJson(const String& path, const String& json);
  bool removeNode(const String& path);

  bool stream(const char* path, StreamCb cb);

private:
  static FirebaseSync* _instance;
  static void _streamEventCb(AsyncResult& aResult);

  WiFiClientSecure _sslClient;
  WiFiClientSecure _streamSslClient;
  AsyncClientClass _aClient;
  AsyncClientClass _streamClient;
  firebase_ns::FirebaseApp _app;
  RealtimeDatabase _rtdb;

  const char* _streamPath = nullptr;
  StreamCb _streamCb = nullptr;
  bool _streamStarted = false;
};
