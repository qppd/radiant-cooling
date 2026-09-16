# Firebase Realtime Database + Firebase-ESP-Client (Mobizt v4.4.17)

> The cloud store shared by the gateway and the Flutter app, and the async
> C++ client library used on the ESP32.

## Firebase Realtime Database

- NoSQL JSON tree, synced in real time over a single HTTPS connection.
- Every node is a URL path: `https://<project>.firebaseio.com/radiant/...`.
- **Streaming:** clients can listen to a path and receive events whenever any
  value under it changes (this replaces polling).
- **Security rules** decide who can read/write each path — see
  `docs/api.md §8`. Ready-to-use rules for the `radiant/` tree:
  `docs/firebase-security-rules.json` (gateway = email/password account,
  identified by `auth.token.email`; app = any authenticated user).

## Firebase-ESP-Client library (Mobizt v4.4.17)

- Install via Arduino Library Manager as **"Firebase Arduino Client Library
  for ESP8266 and ESP32"** by Mobizt, version **4.4.17**.
- **Async, non-blocking:** requests are queued and processed in the
  background; call `Firebase.loop()` every main-loop cycle.
- Auth in this project: **email/password** (required: security rules
  identify the gateway by `auth.token.email`).

### Core pattern

```cpp
FirebaseData fbdo;                 // regular requests
FirebaseData streamFbdo;           // stream connection
FirebaseAuth auth;
FirebaseConfig config;

config.database_url = DATABASE_URL;
config.api_key = API_KEY;
config.auth.token.email = EMAIL;
config.auth.token.password = PASSWORD;

Firebase.begin(&config, &auth);
Firebase.reconnectWiFi(true);

// Write
Firebase.setJSON(fbdo, "/path", jsonStr);        // overwrite
Firebase.updateNode(fbdo, "/path", jsonStr);     // partial update
Firebase.deleteNode(fbdo, "/path");              // delete

// Stream
streamFbdo.setStreamCallback(callback, timeoutCallback, 1000);
Firebase.beginStream(streamFbdo, "/path");

void loop() {
  Firebase.loop();
  Firebase.readStream(&streamFbdo);  // keep stream alive
}
```

Stream callback: `result.dataPath()` for the changed node,
`result.stringData()` for the JSON payload.

## How it is used here

- `FirebaseSync` module (gateway only): `setJson()` / `updateJson()` write
  `radiant/telemetry/*` and `radiant/state/*`; `stream()` listens to
  `radiant/config` in real time. The stream auto-starts in `loop()` once the
  async sign-in completes and re-arms on reconnect.
- The gateway must sign in with **email/password** (not anonymous) for the
  `firebase-security-rules.json` writer role to match (`auth.token.email`).
- `docs/api.md` defines the full data schema.

## Links

- Firebase RTDB docs: <https://firebase.google.com/docs/database>
- Mobizt Firebase-ESP-Client: <https://github.com/mobizt/Firebase-ESP-Client>
- RNT — ESP32 + Firebase RTDB: <https://randomnerdtutorials.com/esp32-firebase-realtime-database/>
