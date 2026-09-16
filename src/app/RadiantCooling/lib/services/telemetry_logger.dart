import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:path_provider/path_provider.dart';

class TelemetryPoint {
  const TelemetryPoint({
    required this.timestamp,
    this.supplyC,
    this.returnC,
    this.coldestPipeC,
    this.tankTempC,
    this.indoorTempC,
    this.indoorHumidityPct,
    this.outdoorTempC,
    this.outdoorDewPointC,
  });

  final DateTime timestamp;
  final double? supplyC;
  final double? returnC;
  final double? coldestPipeC;
  final double? tankTempC;
  final double? indoorTempC;
  final double? indoorHumidityPct;
  final double? outdoorTempC;
  final double? outdoorDewPointC;

  Map<String, dynamic> toJson() => {
    'ts': timestamp.millisecondsSinceEpoch,
    if (supplyC != null) 'supply_c': supplyC,
    if (returnC != null) 'return_c': returnC,
    if (coldestPipeC != null) 'coldest_pipe_c': coldestPipeC,
    if (tankTempC != null) 'tank_c': tankTempC,
    if (indoorTempC != null) 'indoor_temp_c': indoorTempC,
    if (indoorHumidityPct != null) 'indoor_humidity_pct': indoorHumidityPct,
    if (outdoorTempC != null) 'outdoor_temp_c': outdoorTempC,
    if (outdoorDewPointC != null) 'outdoor_dewpoint_c': outdoorDewPointC,
  };

  factory TelemetryPoint.fromJson(Map<String, dynamic> json) =>
      TelemetryPoint(
        timestamp: DateTime.fromMillisecondsSinceEpoch(json['ts'] as int),
        supplyC: (json['supply_c'] as num?)?.toDouble(),
        returnC: (json['return_c'] as num?)?.toDouble(),
        coldestPipeC: (json['coldest_pipe_c'] as num?)?.toDouble(),
        tankTempC: (json['tank_c'] as num?)?.toDouble(),
        indoorTempC: (json['indoor_temp_c'] as num?)?.toDouble(),
        indoorHumidityPct: (json['indoor_humidity_pct'] as num?)?.toDouble(),
        outdoorTempC: (json['outdoor_temp_c'] as num?)?.toDouble(),
        outdoorDewPointC: (json['outdoor_dewpoint_c'] as num?)?.toDouble(),
      );
}

class TelemetryLogger {
  TelemetryLogger({File? file}) : _file = file;

  File? _file;
  static const _maxAge = Duration(days: 7);
  static const _maxPoints = 20160;

  Future<File> _getFile() async {
    if (_file != null) return _file!;
    final dir = await getApplicationDocumentsDirectory();
    _file = File('${dir.path}/telemetry_log.json');
    return _file!;
  }

  Future<List<TelemetryPoint>> load() async {
    try {
      final file = await _getFile();
      if (!await file.exists()) return const [];
      final content = await file.readAsString();
      if (content.isEmpty) return const [];
      final list = jsonDecode(content) as List;
      return [for (final e in list) TelemetryPoint.fromJson(e as Map<String, dynamic>)];
    } catch (_) {
      return const [];
    }
  }

  Future<void> log(TelemetryPoint point) async {
    final points = List<TelemetryPoint>.from(await load());
    points.add(point);

    final cutoff = DateTime.now().subtract(_maxAge);
    points.removeWhere((p) => p.timestamp.isBefore(cutoff));

    if (points.length > _maxPoints) {
      points.removeRange(0, points.length - _maxPoints);
    }

    final file = await _getFile();
    final json = jsonEncode([for (final p in points) p.toJson()]);
    await file.writeAsString(json);
  }

  Future<List<TelemetryPoint>> loadWindow(Duration window) async {
    final all = await load();
    final cutoff = DateTime.now().subtract(window);
    return [for (final p in all) if (!p.timestamp.isBefore(cutoff)) p];
  }

  Future<void> clear() async {
    final file = await _getFile();
    if (await file.exists()) await file.delete();
  }
}
