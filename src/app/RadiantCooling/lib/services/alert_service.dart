import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:path_provider/path_provider.dart';

import '../models/alert.dart';

class AlertService {
  AlertService({File? file}) : _file = file;

  File? _file;
  static const _maxAge = Duration(days: 7);

  bool? _prevGatewayOnline;
  double? _prevColdestPipeC;
  double? _prevWaterFloorC;

  Future<File> _getFile() async {
    if (_file != null) return _file!;
    final dir = await getApplicationDocumentsDirectory();
    _file = File('${dir.path}/alert_log.json');
    return _file!;
  }

  void processHeartbeat({required bool online, required String deviceId}) {
    if (_prevGatewayOnline == true && online == false) {
      addAlert(Alert(
        type: AlertType.gatewayOffline,
        severity: AlertSeverity.critical,
        title: 'Gateway offline',
        message: 'Device $deviceId is no longer responding.',
        timestamp: DateTime.now(),
      ));
    }
    _prevGatewayOnline = online;
  }

  void processMonitorTelemetry({
    double? coldestPipeC,
    double? waterFloorC,
  }) {
    if (coldestPipeC != null && coldestPipeC <= -100 &&
        (_prevColdestPipeC == null || _prevColdestPipeC! > -100)) {
      addAlert(Alert(
        type: AlertType.sensorFailure,
        severity: AlertSeverity.critical,
        title: 'Sensor failure',
        message: 'Pipe temperature sensor returned invalid reading.',
        timestamp: DateTime.now(),
      ));
    }

    if (coldestPipeC != null && waterFloorC != null &&
        coldestPipeC < waterFloorC && coldestPipeC > -100) {
      addAlert(Alert(
        type: AlertType.condensationRisk,
        severity: AlertSeverity.warning,
        title: 'Condensation risk',
        message:
            'Coldest pipe (${coldestPipeC.toStringAsFixed(1)} °C) is below '
            'the water floor (${waterFloorC.toStringAsFixed(1)} °C).',
        timestamp: DateTime.now(),
      ));
    }

    _prevColdestPipeC = coldestPipeC;
    _prevWaterFloorC = waterFloorC;
  }

  void processWeather({required bool valid}) {
    if (!valid && _prevGatewayOnline == true) {
      addAlert(Alert(
        type: AlertType.weatherStale,
        severity: AlertSeverity.warning,
        title: 'Weather data stale',
        message:
            'Outdoor weather data is unavailable. Cooling falls back to '
            'indoor dew point only.',
        timestamp: DateTime.now(),
      ));
    }
  }

  void recordLinked(String systemId) {
    addAlert(Alert(
      type: AlertType.systemLinked,
      severity: AlertSeverity.info,
      title: 'System linked',
      message: 'App linked to system $systemId.',
      timestamp: DateTime.now(),
    ));
  }

  Future<List<Alert>> load() async {
    try {
      final file = await _getFile();
      if (!await file.exists()) return const [];
      final content = await file.readAsString();
      if (content.isEmpty) return const [];
      final list = jsonDecode(content) as List;
      final alerts = [
        for (final e in list) Alert.fromJson(e as Map<String, dynamic>),
      ];
      alerts.sort((a, b) => b.timestamp.compareTo(a.timestamp));
      return alerts;
    } catch (_) {
      return const [];
    }
  }

  Future<void> addAlert(Alert alert) async {
    final alerts = await load();
    alerts.insert(0, alert);

    final cutoff = DateTime.now().subtract(_maxAge);
    alerts.removeWhere((a) => a.timestamp.isBefore(cutoff));

    final file = await _getFile();
    final json = jsonEncode([for (final a in alerts) a.toJson()]);
    await file.writeAsString(json);
  }

  Future<void> clear() async {
    _prevGatewayOnline = null;
    _prevColdestPipeC = null;
    _prevWaterFloorC = null;
    final file = await _getFile();
    if (await file.exists()) await file.delete();
  }

  void resetState() {
    _prevGatewayOnline = null;
    _prevColdestPipeC = null;
    _prevWaterFloorC = null;
  }
}
