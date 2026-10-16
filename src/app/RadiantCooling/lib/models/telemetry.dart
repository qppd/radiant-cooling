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
    this.mode,
    this.ts,
  });

  final double? supplyC;
  final double? returnC;
  final double? coldestPipeC;
  final double? deltaTC;

  final List<double?> tempsC;

  final double? outdoorTempC;
  final double? outdoorDewPointC;
  final double? outdoorHumidityPct;
  final double? dewPointC;
  final double? waterFloorC;
  final bool pumpsOn;

  /// 'auto' or 'manual'; null before the gateway has reported any mode.
  final String? mode;

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
        mode: m?['mode'] as String?,
        ts: m?['ts'] is int ? m!['ts'] as int : null,
      );
}

class ChillerTelemetry {
  const ChillerTelemetry({
    this.outgoingTempC,
    this.ingoingTempC,
    this.waterTempC,
    this.compressorOn = false,
    this.pump1On = false,
    this.ts,
  });

  final double? outgoingTempC;
  final double? ingoingTempC;
  final double? waterTempC;
  final bool compressorOn;
  final bool pump1On;
  final int? ts;

  factory ChillerTelemetry.fromMap(Map<dynamic, dynamic>? m) =>
      ChillerTelemetry(
        outgoingTempC: _num(m, 'outgoing_temp_c'),
        ingoingTempC: _num(m, 'ingoing_temp_c'),
        waterTempC: _num(m, 'water_temp_c'),
        compressorOn: _flag(m, 'compressor', 'on'),
        pump1On: _flag(m, 'pump1', 'on'),
        ts: m?['ts'] is int ? m!['ts'] as int : null,
      );
}

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

class DhState {
  const DhState({this.on = false});

  final bool on;

  factory DhState.fromMap(Map<dynamic, dynamic>? m) =>
      DhState(on: _flag(m, 'dehumidifier', 'on'));
}

class ChillerState {
  const ChillerState({this.compressorOn = false, this.pump1On = false});

  final bool compressorOn;
  final bool pump1On;

  factory ChillerState.fromMap(Map<dynamic, dynamic>? m) => ChillerState(
        compressorOn: _flag(m, 'compressor', 'on'),
        pump1On: _flag(m, 'pump1', 'on'),
      );
}

/// Outdoor conditions the gateway publishes to
/// `radiant/telemetry/weather/latest` right after each WeatherAPI fetch.
class WeatherNow {
  const WeatherNow({
    this.location,
    this.tempC,
    this.feelsLikeC,
    this.humidityPct,
    this.dewPointC,
    this.windKph,
    this.condition,
    this.isDay = true,
    this.obsEpoch,
    this.ts,
  });

  final String? location;
  final double? tempC;
  final double? feelsLikeC;
  final double? humidityPct;
  final double? dewPointC;
  final double? windKph;
  final String? condition;
  final bool isDay;

  /// When WeatherAPI observed the reading (epoch seconds).
  final int? obsEpoch;

  /// When the gateway published it (epoch seconds).
  final int? ts;

  /// True when the node carried no usable reading yet.
  bool get isEmpty => tempC == null && dewPointC == null;

  factory WeatherNow.fromMap(Map<dynamic, dynamic>? m) => WeatherNow(
        location: m?['location'] as String?,
        tempC: _num(m, 'temp_c'),
        feelsLikeC: _num(m, 'feels_like_c'),
        humidityPct: _num(m, 'humidity_pct'),
        dewPointC: _num(m, 'dew_point_c'),
        windKph: _num(m, 'wind_kph'),
        condition: m?['condition'] as String?,
        isDay: m?['is_day'] != false,
        obsEpoch: m?['obs_epoch'] is int ? m!['obs_epoch'] as int : null,
        ts: m?['ts'] is int ? m!['ts'] as int : null,
      );
}

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
