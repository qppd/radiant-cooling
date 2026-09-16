import 'package:shared_preferences/shared_preferences.dart';

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
