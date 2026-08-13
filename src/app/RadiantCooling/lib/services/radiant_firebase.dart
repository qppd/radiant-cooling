import 'package:firebase_database/firebase_database.dart';

/// Firebase Realtime Database access for the app:
///
/// - delivers the WeatherAPI key to `radiant/config/weather_key` (the
///   gateway streams this path and uses the key for its OWN WeatherAPI
///   calls — the ESP32 fetches the weather, the app only manages the key);
/// - discovers/validates systems via the device registry
///   (`radiant/devices/<system_id>`, written by the gateway);
/// - exposes the telemetry/state/heartbeat paths for the linked system.
///
/// Requires `Firebase.initializeApp()` to have completed first (see `main`).
class RadiantFirebase {
  RadiantFirebase({FirebaseDatabase? database})
      : _db = database ?? FirebaseDatabase.instance;

  final FirebaseDatabase _db;

  DatabaseReference get _devicesRef => _db.ref('radiant/devices');

  /// Send the WeatherAPI key to the gateway. The gateway keeps it in RAM
  /// only, so the app should re-send it whenever the system is (re)linked
  /// or the app starts.
  Future<void> publishWeatherKey(String apiKey) async {
    await _db.ref('radiant/config/weather_key').set(apiKey);
  }

  /// List known systems — each registry key is a `SYSTEM_ID` the user can
  /// link to.
  Future<List<String>> discoverSystems() async {
    final snap = await _devicesRef.get();
    if (!snap.exists) return const [];
    final map = snap.value as Map<dynamic, dynamic>? ?? const {};
    return map.keys.cast<String>().toList()..sort();
  }

  /// True when a system with this ID exists in the registry.
  Future<bool> isKnownSystem(String systemId) async {
    final snap = await _devicesRef.child(systemId).get();
    return snap.exists;
  }

  /// Latest telemetry node for a device (`monitor`, `chiller`, `dh`).
  DatabaseReference telemetry(String device) =>
      _db.ref('radiant/telemetry/$device/latest');

  /// Actuator state node for a device.
  DatabaseReference stateOf(String device) => _db.ref('radiant/state/$device');

  /// Gateway heartbeat (includes `device_id` and connectivity).
  DatabaseReference heartbeat() => _db.ref('radiant/heartbeat/monitor');
}
