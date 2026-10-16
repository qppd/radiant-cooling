import 'dart:async';

import 'package:firebase_database/firebase_database.dart';

import '../models/telemetry.dart';

class RadiantFirebase {
  RadiantFirebase({FirebaseDatabase? database}) : _database = database;

  final FirebaseDatabase? _database;

  FirebaseDatabase get _db => _database ?? FirebaseDatabase.instance;

  DatabaseReference get _devicesRef => _db.ref('radiant/devices');

  // Control root for the linked system (writable by the app)
  DatabaseReference get _controlRef =>
      _db.ref('radiant/devices/$_linkedId/control');

  String? _linkedId;

  void setLinkedId(String id) {
    _linkedId = id;
  }

  DatabaseReference get controlRef => _controlRef;

  Future<void> publishWeatherKey(String apiKey) async {
    await _db.ref('radiant/config/weather_key').set(apiKey);
  }

  /// The weather key currently live in Firebase (set previously or from
  /// another device). Null when none is stored.
  Stream<String?> weatherKeyStream() => _cached<String?>(
        'radiant/config/weather_key',
        (e) {
          final v = e.snapshot.value?.toString();
          return (v == null || v.isEmpty) ? null : v;
        },
      );

  Future<String?> fetchWeatherKey() async {
    final snap = await _db.ref('radiant/config/weather_key').get();
    final v = snap.value?.toString();
    return (v == null || v.isEmpty) ? null : v;
  }

  Future<List<String>> discoverSystems() async {
    final snap = await _devicesRef.get();
    if (!snap.exists) return const [];
    final map = snap.value as Map<dynamic, dynamic>? ?? const {};
    final systems = <String>[];
    for (final entry in map.entries) {
      final value = entry.value;
      // The chiller/dehumidifier registry entries the watchdog maintains are
      // tagged role: 'peer' - they belong to a system, they are not one.
      final isPeer = value is Map && value['role'] == 'peer';
      if (!isPeer) systems.add(entry.key.toString());
    }
    return systems..sort();
  }

  Future<bool> isKnownSystem(String systemId) async {
    final snap = await _devicesRef.child(systemId).get();
    return snap.exists;
  }

  // ── Control stream writes (the app → firmware path) ───────────────────
  /// Set pumps on or off via the control stream.
  Future<void> setPumps(bool on) async {
    await _controlRef.update({'pumps': on ? 'on' : 'off'});
  }

  /// Switch between 'auto' and 'manual' mode.
  Future<void> setMode(String mode) async {
    assert(mode == 'auto' || mode == 'manual');
    await _controlRef.update({'mode': mode});
  }

  // ── Reads ──────────────────────────────────────────────────────────────

  DatabaseReference telemetry(String device) =>
      _db.ref('radiant/telemetry/$device/latest');

  DatabaseReference stateOf(String device) => _db.ref('radiant/state/$device');

  DatabaseReference heartbeat() => _db.ref('radiant/heartbeat/monitor');

  // ── Streams ────────────────────────────────────────────────────────────
  // Every active `onValue` listener holds one concurrent RTDB connection, and
  // `asBroadcastStream()` only multiplexes listeners on the single Stream
  // object it is called on - so returning a fresh query from each getter gave
  // one connection per StreamBuilder, re-created on every rebuild (the
  // dashboard alone opened ~10). One cached broadcast stream per path means
  // N widgets share 1 connection.
  final Map<String, Stream<Object?>> _streams = {};

  Stream<T> _cached<T>(
    String path,
    T Function(DatabaseEvent event) parse,
  ) =>
      _streams.putIfAbsent(
        path,
        () => _db.ref(path).onValue.map(parse).asBroadcastStream(),
      ) as Stream<T>;

  Stream<MonitorTelemetry> monitorStream() =>
      _cached('radiant/telemetry/monitor/latest',
          (e) => MonitorTelemetry.fromMap(_map(e)));

  Stream<ChillerTelemetry> chillerStream() =>
      _cached('radiant/telemetry/chiller/latest',
          (e) => ChillerTelemetry.fromMap(_map(e)));

  Stream<DhTelemetry> dhStream() =>
      _cached('radiant/telemetry/dh/latest',
          (e) => DhTelemetry.fromMap(_map(e)));

  Stream<DhState> dhStateStream() =>
      _cached('radiant/state/dh', (e) => DhState.fromMap(_map(e)));

  Stream<ChillerState> chillerStateStream() =>
      _cached('radiant/state/chiller', (e) => ChillerState.fromMap(_map(e)));

  Stream<Heartbeat> heartbeatStream() =>
      _cached('radiant/heartbeat/monitor', (e) => Heartbeat.fromMap(_map(e)));

  Stream<ControlParams> controlParamsStream() =>
      _cached('radiant/config/control/params',
          (e) => ControlParams.fromMap(_map(e)));

  Stream<DhConfig> dhConfigStream() =>
      _cached('radiant/config/dh', (e) => DhConfig.fromMap(_map(e)));

  /// The node the app writes and the gateway streams
  /// (`radiant/devices/<id>/control`). Streaming it back means a pump or mode
  /// toggle shows up in the UI immediately instead of after the next 30 s
  /// telemetry cycle.
  Stream<ControlState> controlStream(String systemId) => _cached(
        'radiant/devices/$systemId/control',
        (e) => ControlState.fromMap(_map(e)),
      );

  /// Outdoor weather the gateway fetched from WeatherAPI.
  Stream<WeatherNow> weatherStream() =>
      _cached('radiant/telemetry/weather/latest',
          (e) => WeatherNow.fromMap(_map(e)));

  // ── Device registry liveness ───────────────────────────────────────────
  /// Record whether [device] is alive. Written by the app watchdog, because a
  /// dead gateway cannot report its own death; `update` merges so the fields a
  /// device publishes itself (firmware, channel, device_id) survive.
  Future<void> setDeviceOnline(
    String device, {
    required bool online,
    String? role,
    String? parent,
    int? lastSeenTs,
  }) {
    final now = DateTime.now().millisecondsSinceEpoch ~/ 1000;
    return _db.ref('radiant/devices/$device').update(<String, Object?>{
      'online': online,
      'ts': now,
      'role': ?role,
      'parent': ?parent,
      'last_seen_ts': ?lastSeenTs,
      // null deletes the key once the device is back.
      'offline_since': online ? null : now,
    });
  }

  /// One-shot read for dialogs. Prefer this over `controlParamsStream().first`:
  /// a shared broadcast stream does not replay its last value to a late
  /// subscriber, so `.first` could hang until the next database change.
  Future<ControlParams> fetchControlParams() async {
    final snap = await configControlRef.get();
    return ControlParams.fromMap(snap.value as Map<dynamic, dynamic>?);
  }

  Future<DhConfig> fetchDhConfig() async {
    final snap = await configDhRef.get();
    return DhConfig.fromMap(snap.value as Map<dynamic, dynamic>?);
  }

  DatabaseReference get configControlRef =>
      _db.ref('radiant/config/control/params');

  DatabaseReference get configDhRef => _db.ref('radiant/config/dh');

  Future<void> updateControlParams({
    double? comfortSetpointC,
    double? dewpointMarginC,
    double? weatherCoolTempC,
  }) async {
    final map = <String, Object>{};
    if (comfortSetpointC != null) map['comfort_setpoint_c'] = comfortSetpointC;
    if (dewpointMarginC != null) map['dewpoint_margin_c'] = dewpointMarginC;
    if (weatherCoolTempC != null) map['weather_cool_temp_c'] = weatherCoolTempC;
    if (map.isNotEmpty) await configControlRef.update(map);
  }

  Future<void> updateDhConfig({
    double? humiditySetpointPct,
    double? humidityDeadbandPct,
  }) async {
    final map = <String, Object>{};
    if (humiditySetpointPct != null) {
      map['humidity_setpoint_pct'] = humiditySetpointPct;
    }
    if (humidityDeadbandPct != null) {
      map['humidity_deadband_pct'] = humidityDeadbandPct;
    }
    if (map.isNotEmpty) await configDhRef.update(map);
  }

  static Map<dynamic, dynamic>? _map(DatabaseEvent e) {
    final v = e.snapshot.value;
    return v is Map<dynamic, dynamic> ? v : null;
  }
}

double? _num(Map<dynamic, dynamic>? m, String key) {
  final v = m?[key];
  return v is num ? v.toDouble() : null;
}

class ControlParams {
  const ControlParams({
    this.comfortSetpointC = 24,
    this.dewpointMarginC = 2,
    this.weatherCoolTempC = 28,
  });

  final double comfortSetpointC;
  final double dewpointMarginC;
  final double weatherCoolTempC;

  factory ControlParams.fromMap(Map<dynamic, dynamic>? m) => ControlParams(
        comfortSetpointC: _num(m, 'comfort_setpoint_c') ?? 24,
        dewpointMarginC: _num(m, 'dewpoint_margin_c') ?? 2,
        weatherCoolTempC: _num(m, 'weather_cool_temp_c') ?? 28,
      );
}

/// State of the app-writable control node (`.../control`).
class ControlState {
  const ControlState({this.pumpsOn = false, this.mode});

  final bool pumpsOn;

  /// 'auto' or 'manual'; null when nothing has been commanded yet.
  final String? mode;

  factory ControlState.fromMap(Map<dynamic, dynamic>? m) => ControlState(
        pumpsOn: m?['pumps'] == 'on' || m?['pumps'] == true,
        mode: m?['mode'] as String?,
      );
}

class DhConfig {
  const DhConfig({this.humiditySetpointPct = 55, this.humidityDeadbandPct = 5});

  final double humiditySetpointPct;
  final double humidityDeadbandPct;
  factory DhConfig.fromMap(Map<dynamic, dynamic>? m) => DhConfig(
        humiditySetpointPct: _num(m, 'humidity_setpoint_pct') ?? 55,
        humidityDeadbandPct: _num(m, 'humidity_deadband_pct') ?? 5,
      );
}
