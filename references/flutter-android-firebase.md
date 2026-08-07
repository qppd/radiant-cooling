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

- Packages: `firebase_core` (mandatory) + `firebase_database` (RTDB).
- Android setup: add `google-services.json` and apply the Google Services
  Gradle plugin, plus the `INTERNET` permission in the manifest.
- The app reads `radiant/telemetry/*`, `radiant/state/*`, and writes
  `radiant/config/*` — the path scheme is in `docs/api.md`.

## How it is used here

- The app UI/dashboard and Firebase wiring are **next on the roadmap** —
  only the Flutter scaffold exists so far.

## Links

- Flutter: <https://flutter.dev>
- Firebase Flutter docs: <https://firebase.flutter.dev>
- firebase_database package: <https://pub.dev/packages/firebase_database>
- firebase_core package: <https://pub.dev/packages/firebase_core>
