/// App configuration.
///
/// Copy this file to `lib/config/app_config.dart` and fill in the values.
/// `app_config.dart` is git-ignored, so secrets (like the WeatherAPI key)
/// stay local and are never committed.
///
/// The WeatherAPI key is managed HERE, in the app. The **monitor ESP32**
/// fetches the weather itself; the app only delivers the key to the gateway
/// at runtime via `radiant/config/weather_key` (Firebase) — it is never
/// compiled into the firmware.
class AppConfig {
  AppConfig._();

  /// WeatherAPI.com API key (https://www.weatherapi.com).
  /// Pre-fills the key entry screen; can also be pasted there directly.
  static const String weatherApiKey = 'your-weatherapi-key';

  // ---- Firebase (Firebase console -> Project settings -> General) ----
  // The app calls `Firebase.initializeApp(options: ...)` with these values,
  // so it works on any platform without a google-services.json. Copy the
  // values from your project's config (or `references/firebase-creds.txt`
  // if you saved it there). The Web API key is a public client identifier
  // (access is enforced by the security rules), so embedding it is fine.
  static const String firebaseApiKey = 'your-web-api-key';
  static const String firebaseAppId = '1:123456789:web:abcdef1234567890';
  static const String firebaseMessagingSenderId = '123456789';
  static const String firebaseProjectId = 'your-project-id';
  static const String firebaseAuthDomain = 'your-project-id.firebaseapp.com';
  static const String firebaseDatabaseUrl =
      'https://your-project-id-default-rtdb.firebaseio.com';
  static const String firebaseStorageBucket =
      'your-project-id.firebasestorage.app';

  /// Default system ID pre-filled in the link screen.
  /// Must match `SYSTEM_ID` in the gateway's `Config.h`
  /// (e.g. `RADIANT-001`) so the app links to the right installation.
  static const String defaultSystemId = 'RADIANT-001';
}
