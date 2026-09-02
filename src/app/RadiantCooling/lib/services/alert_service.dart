import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:path_provider/path_provider.dart';

import '../models/alert.dart';

/// Detects system events from Firebase stream changes and persists them
/// as [Alert] entries for the Alerts screen.
///
/// Call [processHeartbeat], [processMonitorTelemetry], etc. from
/// StreamBuilder callbacks or stream listeners.
class AlertService {
  AlertService({File? file}) : _file = file;

  File? _file;
  static const _maxAge = Duration(days: 7);

  // Track previous states to detect transitions.
  bool? _prevGatewayOnline;
  double? _prevColdestPipeC;
  double? _prevWaterFloorC;

  Future<File> _getFile() async {
    if (_file != null) return _file!;
    final dir = await getApplicationDocumentsDirectory();
    _file = File('${dir.path}/alert_log.json');
    return _file!;
  }

  /// Process a heartbeat update — detect gateway going offline.
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

  /// Process monitor telemetry — detect sensor failures and condensation risk.
  void processMonitorTelemetry({
    double? coldestPipeC,
    double? waterFloorC,
  }) {
    // Sensor failure: coldest pipe dropped to invalid range.
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

    // Condensation risk: coldest pipe below water floor.
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

  /// Process outdoor weather — detect stale weather data.
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

  /// Manually record a system-linked event.
  void recordLinked(String systemId) {
    addAlert(Alert(
      type: AlertType.systemLinked,
      severity: AlertSeverity.info,
      title: 'System linked',
      message: 'App linked to system $systemId.',
      timestamp: DateTime.now(),
    ));
  }

  /// Load all stored alerts (newest first).
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
      // Newest first.
      alerts.sort((a, b) => b.timestamp.compareTo(a.timestamp));
      return alerts;
    } catch (_) {
      return const [];
    }
  }

  /// Append an alert and prune old entries.
  Future<void> addAlert(Alert alert) async {
    final alerts = await load();
    alerts.insert(0, alert); // newest first

    // Prune entries older than _maxAge.
    final cutoff = DateTime.now().subtract(_maxAge);
    alerts.removeWhere((a) => a.timestamp.isBefore(cutoff));

    final file = await _getFile();
    final json = jsonEncode([for (final a in alerts) a.toJson()]);
    await file.writeAsString(json);
  }

  /// Clear all alerts.
  Future<void> clear() async {
    _prevGatewayOnline = null;
    _prevColdestPipeC = null;
    _prevWaterFloorC = null;
    final file = await _getFile();
    if (await file.exists()) await file.delete();
  }

  /// Reset detection state (e.g. on re-link).
  void resetState() {
    _prevGatewayOnline = null;
    _prevColdestPipeC = null;
    _prevWaterFloorC = null;
  }
}
