/// Typed views of the Firebase `radiant/*` nodes used by the dashboard.
///
/// Each `fromMap` tolerates missing keys (returns nulls/0) so the UI can
/// render whatever the gateway currently publishes.
library;

double? _num(Map<dynamic, dynamic>? m, String key) {
  final v = m?[key];
  return v is num ? v.toDouble() : null;
}

bool _flag(Map<dynamic, dynamic>? m, String key, String onValue) {
  return m?[key] == onValue;
}

List<double?> _list(Map<dynamic, dynamic>? m, String key) {
  final v = m?[key];
  if (v is! List) return const [];
  return [
    for (final e in v)
      if (e is num) e.toDouble(),
  ];
}

/// `radiant/telemetry/monitor/latest`
class MonitorTelemetry {
  const MonitorTelemetry({
    this.supplyC,
    this.returnC,
    this.coldestPipeC,
    this.deltaTC,
    this.tempsC = const [],
    this.outdoorTempC,
    this.outdoorDewPointC,
    this.outdoorHumidityPct,
    this.dewPointC,
    this.waterFloorC,
    this.pumpsOn = false,
    this.ts,
  });

  final double? supplyC;
  final double? returnC;
  final double? coldestPipeC;
  final double? deltaTC;

  /// Raw DS18B20 readings: `[supply, return, pipe1..pipe4]`.
  final List<double?> tempsC;

  final double? outdoorTempC;
  final double? outdoorDewPointC;
  final double? outdoorHumidityPct;
  final double? dewPointC;
  final double? waterFloorC;
  final bool pumpsOn;
  final int? ts;

  factory MonitorTelemetry.fromMap(Map<dynamic, dynamic>? m) =>
      MonitorTelemetry(
        supplyC: _num(m, 'supply_c'),
        returnC: _num(m, 'return_c'),
        coldestPipeC: _num(m, 'coldest_pipe_c'),
        deltaTC: _num(m, 'delta_t_c'),
        tempsC: _list(m, 'temps_c'),
        outdoorTempC: _num(m, 'outdoor_temp_c'),
        outdoorDewPointC: _num(m, 'outdoor_dewpoint_c'),
        outdoorHumidityPct: _num(m, 'outdoor_humidity_pct'),
        dewPointC: _num(m, 'dew_point_c'),
        waterFloorC: _num(m, 'water_floor_c'),
        pumpsOn: _flag(m, 'pumps', 'on'),
        ts: m?['ts'] is int ? m!['ts'] as int : null,
      );
}

/// `radiant/telemetry/chiller/latest`
class ChillerTelemetry {
  const ChillerTelemetry({
    this.waterTempC,
    this.pump1On = false,
    this.pump2On = false,
    this.ts,
  });

  final double? waterTempC;
  final bool pump1On;
  final bool pump2On;
  final int? ts;

  factory ChillerTelemetry.fromMap(Map<dynamic, dynamic>? m) =>
      ChillerTelemetry(
        waterTempC: _num(m, 'water_temp_c'),
        pump1On: _flag(m, 'pump1', 'on'),
        pump2On: _flag(m, 'pump2', 'on'),
        ts: m?['ts'] is int ? m!['ts'] as int : null,
      );
}

/// `radiant/telemetry/dh/latest`
class DhTelemetry {
  const DhTelemetry({this.tempC, this.humidityPct, this.ts});

  final double? tempC;
  final double? humidityPct;
  final int? ts;

  factory DhTelemetry.fromMap(Map<dynamic, dynamic>? m) => DhTelemetry(
    tempC: _num(m, 'temp_c'),
    humidityPct: _num(m, 'humidity_pct'),
    ts: m?['ts'] is int ? m!['ts'] as int : null,
  );
}

/// `radiant/state/dh` — dehumidifier actuator state.
class DhState {
  const DhState({this.on = false});

  final bool on;

  factory DhState.fromMap(Map<dynamic, dynamic>? m) =>
      DhState(on: _flag(m, 'dehumidifier', 'on'));
}

/// `radiant/state/chiller` — chiller pump actuator states.
class ChillerState {
  const ChillerState({this.pump1On = false, this.pump2On = false});

  final bool pump1On;
  final bool pump2On;

  factory ChillerState.fromMap(Map<dynamic, dynamic>? m) => ChillerState(
    pump1On: _flag(m, 'pump1', 'on'),
    pump2On: _flag(m, 'pump2', 'on'),
  );
}

/// `radiant/heartbeat/monitor` — gateway connectivity + device id.
class Heartbeat {
  const Heartbeat({this.online = false, this.deviceId, this.ts});

  final bool online;
  final String? deviceId;
  final int? ts;

  factory Heartbeat.fromMap(Map<dynamic, dynamic>? m) => Heartbeat(
    online: m?['online'] == true,
    deviceId: m?['device_id'] as String?,
    ts: m?['ts'] is int ? m!['ts'] as int : null,
  );
}
