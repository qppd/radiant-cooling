import 'package:shared_preferences/shared_preferences.dart';

/// Persists the linked system ID so the app reconnects to the same ESP32
/// installation on every launch.
///
/// The user enters the `SYSTEM_ID` configured in the gateway's `Config.h`
/// (published in `radiant/devices/<SYSTEM_ID>`); this service stores it
/// locally.
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
