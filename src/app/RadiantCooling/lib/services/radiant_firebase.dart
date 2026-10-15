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

  Future<List<String>> discoverSystems() async {
    final snap = await _devicesRef.get();
    if (!snap.exists) return const [];
    final map = snap.value as Map<dynamic, dynamic>? ?? const {};
    return map.keys.cast<String>().toList()..sort();
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

  // Each call returns a broadcast stream — FirebaseDatabase.onValue streams are
  // single-subscription by default, but the dashboard uses multiple StreamBuilders
  // and main.dart also listens directly. .asBroadcastStream() lets many subscribers.

  Stream<MonitorTelemetry> monitorStream() =>
      telemetry('monitor').onValue.map((e) => MonitorTelemetry.fromMap(_map(e)))
          .asBroadcastStream();

  Stream<ChillerTelemetry> chillerStream() =>
      telemetry('chiller').onValue.map((e) => ChillerTelemetry.fromMap(_map(e)))
          .asBroadcastStream();

  Stream<DhTelemetry> dhStream() =>
      telemetry('dh').onValue.map((e) => DhTelemetry.fromMap(_map(e)))
          .asBroadcastStream();

  Stream<DhState> dhStateStream() =>
      stateOf('dh').onValue.map((e) => DhState.fromMap(_map(e)))
          .asBroadcastStream();

  Stream<ChillerState> chillerStateStream() =>
      stateOf('chiller').onValue.map((e) => ChillerState.fromMap(_map(e)))
          .asBroadcastStream();

  Stream<Heartbeat> heartbeatStream() =>
      heartbeat().onValue.map((e) => Heartbeat.fromMap(_map(e)))
          .asBroadcastStream();

  Stream<ControlParams> controlParamsStream() =>
      configControlRef.onValue.map((e) => ControlParams.fromMap(_map(e)))
          .asBroadcastStream();

  Stream<DhConfig> dhConfigStream() =>
      configDhRef.onValue.map((e) => DhConfig.fromMap(_map(e)))
          .asBroadcastStream();

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

class DhConfig {
  const DhConfig({this.humiditySetpointPct = 55, this.humidityDeadbandPct = 5});

  final double humiditySetpointPct;
  final double humidityDeadbandPct;
  factory DhConfig.fromMap(Map<dynamic, dynamic>? m) => DhConfig(
        humiditySetpointPct: _num(m, 'humidity_setpoint_pct') ?? 55,
        humidityDeadbandPct: _num(m, 'humidity_deadband_pct') ?? 5,
      );
}
