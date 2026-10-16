#include "FirebaseSync.h"
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Global instance pointer for callback access
FirebaseSync* globalFirebaseSync = nullptr;

// Minimum spacing between re-subscribe attempts for a dead stream.
static const unsigned long kStreamRetryMs = 5000;

FirebaseSync::FirebaseSync() {
    configStreamActive = false;
    controlStreamActive = false;
    configHandler = nullptr;
    controlHandler = nullptr;
    lastConfigRetryMs = 0;
    lastControlRetryMs = 0;
    globalFirebaseSync = this;
}

void FirebaseSync::printNetworkDiagnostics() {
    Serial.println("\n=== Network Diagnostics ===");

    Serial.printf("WiFi Status: %s\n",
                  WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("DNS Server: %s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());

    Serial.println("\nTesting DNS resolution...");
    IPAddress ip;
    if (WiFi.hostByName("pool.ntp.org", ip)) {
        Serial.printf("DNS Test: SUCCESS - pool.ntp.org resolved to %s\n", ip.toString().c_str());
    } else {
        Serial.println("DNS Test: FAILED - Could not resolve pool.ntp.org");
    }

    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("=== End Diagnostics ===\n");
}

void FirebaseSync::begin() {
    printNetworkDiagnostics();

    Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

    // Assign Firebase credentials
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    // User authentication (email/password)
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    // Set network reconnection
    Firebase.reconnectNetwork(true);

    // Configure buffer sizes for ESP32
    fbdo.setBSSLBufferSize(2048, 1024);
    fbdo.setResponseSize(2048);

    // Set timeouts
    config.timeout.serverResponse = 10 * 1000;
    config.timeout.socketConnection = 10 * 1000;
    config.timeout.sslHandshake = 30 * 1000;
    config.timeout.rtdbKeepAlive = 45 * 1000;
    config.timeout.rtdbStreamReconnect = 1 * 1000;
    config.timeout.rtdbStreamError = 3 * 1000;

    // Begin Firebase with retry logic
    Serial.println("Initializing Firebase...");

    int retryCount = 0;
    const int maxRetries = 5;
    const int baseDelay = 2000;

    while (retryCount < maxRetries) {
        Firebase.begin(&config, &auth);

        int initWait = 0;
        const int maxInitWait = 30;

        while (!Firebase.ready() && initWait < maxInitWait) {
            delay(1000);
            initWait++;
            Serial.print(".");
        }

        if (Firebase.ready()) {
            Serial.println("\nFirebase initialized successfully!");
            break;
        } else {
            retryCount++;
            Serial.printf("\nFirebase initialization failed (attempt %d/%d)\n", retryCount, maxRetries);

            if (retryCount < maxRetries) {
                int delayTime = baseDelay * (1 << (retryCount - 1));
                Serial.printf("Retrying in %d seconds...\n", delayTime / 1000);
                delay(delayTime);
            }
        }
    }

    if (!Firebase.ready()) {
        Serial.println("Failed to initialize Firebase after all retries!");
        return;
    }

    // Initialize streams for real-time monitoring
    initializeStreams();
}

void FirebaseSync::setConfigStreamHandler(StreamHandler handler) {
    configHandler = handler;
}

void FirebaseSync::setControlStreamHandler(StreamHandler handler) {
    controlHandler = handler;
}

void FirebaseSync::initializeStreams() {
    Serial.println("Initializing Firebase streams...");
    initializeConfigStream();
    initializeControlStream();
    Serial.println("Firebase streams initialization completed!");
}

void FirebaseSync::initializeConfigStream() {
    String path = RadiantFirebaseConfig::getConfigPath();
    Serial.println("Starting config stream for path: " + path);

    if (!Firebase.RTDB.beginMultiPathStream(&configStream, path)) {
        Serial.printf("Config stream init failed: %s\n", configStream.errorReason().c_str());
        configStreamActive = false;
    } else {
        Firebase.RTDB.setMultiPathStreamCallback(&configStream,
            configStreamCallback, configStreamTimeoutCallback);
        configStreamActive = true;
        Serial.println("Firebase config stream initialized successfully!");
    }
}

void FirebaseSync::initializeControlStream() {
    String path = RadiantFirebaseConfig::getControlPath();
    Serial.println("Starting control stream for path: " + path);

    if (!Firebase.RTDB.beginMultiPathStream(&controlStream, path)) {
        Serial.printf("Control stream init failed: %s\n", controlStream.errorReason().c_str());
        controlStreamActive = false;
    } else {
        Firebase.RTDB.setMultiPathStreamCallback(&controlStream,
            controlStreamCallback, controlStreamTimeoutCallback);
        controlStreamActive = true;
        Serial.println("Firebase control stream initialized successfully!");
    }
}

void FirebaseSync::handleStreams() {
    // Re-subscribe dead streams. The timeout callback only clears the active
    // flag, so without this a single network blip stopped config updates -
    // including the WeatherAPI key delivery - until the next reboot.
    const unsigned long now = millis();
    if (!configStreamActive && now - lastConfigRetryMs >= kStreamRetryMs) {
        lastConfigRetryMs = now;
        Serial.println("Config stream down - resubscribing...");
        initializeConfigStream();
    }
    if (!controlStreamActive && now - lastControlRetryMs >= kStreamRetryMs) {
        lastControlRetryMs = now;
        Serial.println("Control stream down - resubscribing...");
        initializeControlStream();
    }

    // Handle config stream
    if (configStreamActive && !Firebase.RTDB.readStream(&configStream)) {
        Serial.printf("Config stream error: %s\n", configStream.errorReason().c_str());
        if (configStream.httpCode() != FIREBASE_ERROR_HTTP_CODE_OK) {
            Serial.println("Attempting to reinitialize config stream...");
            initializeConfigStream();
        }
    }

    // Handle control stream
    if (controlStreamActive && !Firebase.RTDB.readStream(&controlStream)) {
        Serial.printf("Control stream error: %s\n", controlStream.errorReason().c_str());
        if (controlStream.httpCode() != FIREBASE_ERROR_HTTP_CODE_OK) {
            Serial.println("Attempting to reinitialize control stream...");
            initializeControlStream();
        }
    }
}

void FirebaseSync::loop() {
    handleStreams();
}

bool FirebaseSync::isReady() {
    return WiFi.status() == WL_CONNECTED && Firebase.ready();
}

bool FirebaseSync::setJson(const String& path, const String& json) {
    if (!isReady()) return false;

    FirebaseJson fbJson;
    fbJson.setJsonData(json);

    if (Firebase.RTDB.setJSON(&fbdo, path, &fbJson)) {
        return true;
    } else {
        Serial.printf("setJson failed at %s: %s\n", path.c_str(), fbdo.errorReason().c_str());
        return false;
    }
}

bool FirebaseSync::updateJson(const String& path, const String& json) {
    if (!isReady()) return false;

    FirebaseJson fbJson;
    fbJson.setJsonData(json);

    if (Firebase.RTDB.updateNode(&fbdo, path, &fbJson)) {
        return true;
    } else {
        Serial.printf("updateJson failed at %s: %s\n", path.c_str(), fbdo.errorReason().c_str());
        return false;
    }
}

bool FirebaseSync::removeNode(const String& path) {
    if (!isReady()) return false;

    if (Firebase.RTDB.deleteNode(&fbdo, path)) {
        return true;
    } else {
        Serial.printf("removeNode failed at %s: %s\n", path.c_str(), fbdo.errorReason().c_str());
        return false;
    }
}

bool FirebaseSync::readString(const String& path, String& out) {
    if (!isReady()) return false;

    if (!Firebase.RTDB.getString(&fbdo, path)) {
        Serial.printf("readString failed at %s: %s\n", path.c_str(), fbdo.errorReason().c_str());
        return false;
    }

    out = fbdo.stringData();
    out.trim();
    return out.length() > 0;
}

// ── Config Stream Callbacks ──

void FirebaseSync::configStreamCallback(MultiPathStream stream) {
    if (globalFirebaseSync == nullptr) return;

    Serial.println("[config] Stream Update Received!");
    Serial.println("Path: " + stream.dataPath);

    // Dispatch to user handler registered from .ino
    if (globalFirebaseSync->configHandler) {
        globalFirebaseSync->configHandler(stream);
    }
}

void FirebaseSync::configStreamTimeoutCallback(bool timeout) {
    if (timeout) {
        Serial.println("Config stream timed out, attempting to resume...");
    }
    if (globalFirebaseSync && !globalFirebaseSync->configStream.httpConnected()) {
        Serial.printf("Config Stream Error code: %d, reason: %s\n",
                      globalFirebaseSync->configStream.httpCode(),
                      globalFirebaseSync->configStream.errorReason().c_str());
        globalFirebaseSync->configStreamActive = false;
    }
}

// ── Control Stream Callbacks ──

void FirebaseSync::controlStreamCallback(MultiPathStream stream) {
    if (globalFirebaseSync == nullptr) return;

    Serial.println("[control] Stream Update Received!");
    Serial.println("Path: " + stream.dataPath);

    // Dispatch to user handler registered from .ino
    if (globalFirebaseSync->controlHandler) {
        globalFirebaseSync->controlHandler(stream);
    }
}

void FirebaseSync::controlStreamTimeoutCallback(bool timeout) {
    if (timeout) {
        Serial.println("Control stream timed out, attempting to resume...");
    }
    if (globalFirebaseSync && !globalFirebaseSync->controlStream.httpConnected()) {
        Serial.printf("Control Stream Error code: %d, reason: %s\n",
                      globalFirebaseSync->controlStream.httpCode(),
                      globalFirebaseSync->controlStream.errorReason().c_str());
        globalFirebaseSync->controlStreamActive = false;
    }
}
