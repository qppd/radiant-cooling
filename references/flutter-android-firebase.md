# Flutter Android + Firebase

> The companion app target and its cloud SDK.

## Flutter (Android)

- The `RadiantCooling` app scaffold lives in `src/app/RadiantCooling/`
  (Android target decided for this project).
- Standard workflow:

```bash
cd src/app/RadiantCooling
flutter pub get
flutter run
```

## Firebase SDK

- Packages: `firebase_core` (mandatory) + `firebase_auth` (email/password
  login) + `firebase_database` (RTDB) + `shared_preferences` (local link +
  key storage).
- Android setup: add `google-services.json` and apply the Google Services
  Gradle plugin, plus the `INTERNET` permission in the manifest.
- The app reads `radiant/telemetry/*`, `radiant/state/*`, and writes
  `radiant/config/*` — the path scheme is in `docs/api.md`.

## How it is used here

- **Auth:** `AuthGate` in `lib/main.dart` shows a login/signup screen
  (`lib/screens/auth_screen.dart`) until a user is signed in — the security
  rules require `auth != null` for every read/write.
- **Dashboard** (`lib/screens/dashboard_screen.dart`): streams
  `radiant/telemetry/*`, `radiant/state/*` and `radiant/heartbeat/monitor`
  via `RadiantFirebase` and renders status/weather/loop/control cards.
- **Settings** (`lib/screens/settings_screen.dart`): writes control params
  and the dehumidifier target to `radiant/config/*`, plus system linking,
  the WeatherAPI key, and sign-out.
- The path scheme is in `docs/api.md`.

## Links

- Flutter: <https://flutter.dev>
- Firebase Flutter docs: <https://firebase.flutter.dev>
- firebase_database package: <https://pub.dev/packages/firebase_database>
- firebase_core package: <https://pub.dev/packages/firebase_core>
