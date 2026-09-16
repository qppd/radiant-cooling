import 'package:shared_preferences/shared_preferences.dart';

class DeviceLink {
  static const _key = 'linked_system_id';

  Future<String?> load() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_key);
  }

  Future<void> save(String systemId) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_key, systemId.trim());
  }

  Future<void> clear() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_key);
  }
}
