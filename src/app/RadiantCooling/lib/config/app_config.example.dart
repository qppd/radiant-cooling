/// App configuration.
///
/// Copy this file to `lib/config/app_config.dart` and fill in the values.
/// `app_config.dart` is git-ignored, so secrets (like the WeatherAPI key)
/// stay local and are never committed.
///
/// The WeatherAPI key is managed HERE, in the app — the ESP32 firmware never
/// stores or calls the API. The app polls the current-weather endpoint and
/// writes the result to `radiant/config/weather` in Firebase, which the
/// gateway streams (see `docs/api.md` §4).
class AppConfig {
  AppConfig._();

  /// WeatherAPI.com API key (https://www.weatherapi.com).
  static const String weatherApiKey = 'your-weatherapi-key';

  /// Location query for the current-weather endpoint
  /// (city name, lat/lon, or postal code).
  static const String weatherLocation = 'your-city';

  /// How often the app refreshes outdoor weather (minutes).
  /// Keep conservative — the free tier has a daily call budget.
  static const int weatherPollMinutes = 15;

  /// Default system ID pre-filled in the link screen.
  /// Must match `SYSTEM_ID` in the gateway's `Config.h`
  /// (e.g. `RADIANT-001`) so the app links to the right installation.
  static const String defaultSystemId = 'RADIANT-001';
}
