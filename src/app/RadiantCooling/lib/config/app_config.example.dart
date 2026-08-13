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

  /// Default system ID pre-filled in the link screen.
  /// Must match `SYSTEM_ID` in the gateway's `Config.h`
  /// (e.g. `RADIANT-001`) so the app links to the right installation.
  static const String defaultSystemId = 'RADIANT-001';
}
