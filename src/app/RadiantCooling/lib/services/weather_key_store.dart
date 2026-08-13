import 'package:shared_preferences/shared_preferences.dart';

/// Persists the WeatherAPI key the user entered in the app, so the app can
/// re-deliver it to the gateway after a gateway reboot (the ESP32 keeps the
/// key in RAM only and forgets it on restart).
class WeatherKeyStore {
  static const _key = 'weather_api_key';

  Future<String?> load() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_key);
  }

  Future<void> save(String apiKey) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_key, apiKey.trim());
  }

  Future<void> clear() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_key);
  }
}
