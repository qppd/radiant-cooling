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
