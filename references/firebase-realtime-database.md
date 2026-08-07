# Firebase Realtime Database + FirebaseClient (Mobizt)

> The cloud store shared by the gateway and the Flutter app, and the async
> C++ client library used on the ESP32.

## Firebase Realtime Database

- NoSQL JSON tree, synced in real time over a single HTTPS connection.
- Every node is a URL path: `https://<project>.firebaseio.com/radiant/...`.
- **Streaming:** clients can listen to a path and receive events whenever any
  value under it changes (this replaces polling).
- **Security rules** decide who can read/write each path — see
  `docs/api.md §8`.

## FirebaseClient library (Mobizt)

- The **current** ESP32 client (the older `Firebase-ESP-Client` is
  **deprecated / end-of-life**). Install via Arduino Library Manager as
  **"FirebaseClient"**.
- **Async, non-blocking:** requests are queued and processed in the
  background; call the library loops every main-loop cycle.
- Auth in this project: **anonymous** (default) or **email/password** —
  whichever provider is enabled in the Firebase console.

### Core pattern

```cpp
DefaultNetwork   network;
FirebaseApp      app;
AsyncClientClass aClient(network);
RealtimeDatabase rtdb;

UserAuth auth(API_KEY);                          // anonymous
// UserAuth auth(API_KEY, "user@mail.com", "pass"); // email/password

initializeApp(aClient, app, getAuth(auth), aClient.loop);
app.getApp<RealtimeDatabase>(rtdb);
rtdb.url(DATABASE_URL);

rtdb.set<String>(aClient, "/path", "{...}");     // write JSON
rtdb.update<String>(aClient, "/path", "{...}");  // partial update
rtdb.stream(aClient, "/path", eventCb, timeoutCb); // realtime listener

void loop() {
  network.loop();
  app.loop();
  aClient.loop();
  // queue writes only when app.ready() && !aClient.isBusy()
}
```

Stream event callback: `aResult.isStream() && aResult.isData()`, payload via
`aResult.to<String>()`, changed node via `aResult.dataPath()`.

## How it is used here

- `FirebaseSync` module (gateway only): `setJson()` / `updateJson()` write
  `radiant/telemetry/*` and `radiant/state/*`; `stream()` listens to
  `radiant/config` in real time. The stream auto-starts in `loop()` once the
  async sign-in completes and re-arms on reconnect.
- `docs/api.md` defines the full data schema.

## Links

- Firebase RTDB docs: <https://firebase.google.com/docs/database>
- Mobizt FirebaseClient: <https://github.com/mobizt/FirebaseClient>
- Firebase-ESP-Client (deprecated): <https://github.com/mobizt/Firebase-ESP-Client>
- RNT — ESP32 + Firebase RTDB: <https://randomnerdtutorials.com/esp32-firebase-realtime-database/>
