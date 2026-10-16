#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "firebase_credentials.h"
#include "FirebaseConfig.h"

// Forward declaration
class FirebaseSync;

// Global instance pointer for callback access
extern FirebaseSync* globalFirebaseSync;

// User-provided stream handler signature
typedef void (*StreamHandler)(MultiPathStream stream);

class FirebaseSync {
public:
    FirebaseSync();

    void begin();
    void loop();
    bool isReady();

    // Data operations
    bool setJson(const String& path, const String& json);
    bool updateJson(const String& path, const String& json);
    bool removeNode(const String& path);

    // One-shot read of a scalar node (used to pull the WeatherAPI key without
    // depending on the config stream being up).
    bool readString(const String& path, String& out);

    // Register stream handlers (called from .ino)
    void setConfigStreamHandler(StreamHandler handler);
    void setControlStreamHandler(StreamHandler handler);

    // Public Firebase objects for callbacks
    FirebaseData fbdo;
    FirebaseData configStream;
    FirebaseData controlStream;

private:
    FirebaseAuth auth;
    FirebaseConfig config;

    // Stream active states
    bool configStreamActive;
    bool controlStreamActive;

    // Last re-subscribe attempt per stream (a dropped stream used to stay
    // dead forever, which silently stopped config changes).
    unsigned long lastConfigRetryMs;
    unsigned long lastControlRetryMs;

    // User-provided handlers
    StreamHandler configHandler;
    StreamHandler controlHandler;

    // Internal methods
    void initializeStreams();
    void initializeConfigStream();
    void initializeControlStream();
    void handleStreams();
    void printNetworkDiagnostics();

    // Static callbacks for Firebase library
    static void configStreamCallback(MultiPathStream stream);
    static void configStreamTimeoutCallback(bool timeout);
    static void controlStreamCallback(MultiPathStream stream);
    static void controlStreamTimeoutCallback(bool timeout);
};
