#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseESP32.h>

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

  bool setJson(const String& path, const String& json);
  bool updateJson(const String& path, const String& json);
  bool removeNode(const String& path);

  bool stream(const char* path, StreamCb cb);

private:
  static void _streamCallback(FirebaseStreamData result);
  static void _streamTimeoutCallback(bool timeout);
  static FirebaseSync* _instance;

  FirebaseData _fbdo;
  FirebaseData _streamFbdo;
  FirebaseAuth _auth;
  FirebaseConfig _config;

  const char* _streamPath = nullptr;
  StreamCb _streamCb = nullptr;
  bool _streamStarted = false;
};
