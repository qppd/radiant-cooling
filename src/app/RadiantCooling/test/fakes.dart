import 'dart:async';

import 'package:firebase_auth/firebase_auth.dart';
import 'package:radiant_cooling/models/alert.dart';
import 'package:radiant_cooling/models/telemetry.dart';
import 'package:radiant_cooling/services/alert_service.dart';
import 'package:radiant_cooling/services/auth_service.dart';
import 'package:radiant_cooling/services/notification_service.dart';
import 'package:radiant_cooling/services/radiant_firebase.dart';
import 'package:radiant_cooling/services/telemetry_logger.dart';

class FakeAuthService extends AuthService {
  FakeAuthService() : super();

  final _authState = StreamController<User?>.broadcast();

  int signInCalls = 0;
  int signUpCalls = 0;
  String? lastSignInEmail;
  String? lastSignInPassword;
  String? lastSignUpEmail;
  String? lastSignUpPassword;

  Object? signInError;
  Object? signUpError;

  @override
  Stream<User?> get authState => _authState.stream;

  @override
  Future<void> signIn(String email, String password) async {
    signInCalls++;
    lastSignInEmail = email.trim();
    lastSignInPassword = password;
    final error = signInError;
    if (error != null) throw error;
  }

  @override
  Future<void> signUp(String email, String password) async {
    signUpCalls++;
    lastSignUpEmail = email.trim();
    lastSignUpPassword = password;
    final error = signUpError;
    if (error != null) throw error;
  }

  @override
  Future<void> signOut() async {}
}

class FakeRadiantFirebase extends RadiantFirebase {
  FakeRadiantFirebase({
    MonitorTelemetry? monitor,
    ChillerTelemetry? chiller,
    DhTelemetry? dh,
    DhState? dhState,
    ChillerState? chillerState,
    Heartbeat? heartbeat,
    ControlParams? controlParams,
    DhConfig? dhConfig,
  }) : _monitor = monitor,
       _chiller = chiller,
       _dh = dh,
       _dhState = dhState,
       _chillerState = chillerState,
       _heartbeat = heartbeat,
       _controlParams = controlParams,
       _dhConfig = dhConfig,
       super();

  static Stream<T> _value<T>(T? value) =>
      value == null ? Stream<T>.empty() : Stream<T>.value(value);

  final MonitorTelemetry? _monitor;
  final ChillerTelemetry? _chiller;
  final DhTelemetry? _dh;
  final DhState? _dhState;
  final ChillerState? _chillerState;
  final Heartbeat? _heartbeat;
  final ControlParams? _controlParams;
  final DhConfig? _dhConfig;

  ControlParams? lastControlParamsWrite;
  DhConfig? lastDhConfigWrite;

  List<String> knownSystems = const [];

  @override
  Stream<MonitorTelemetry> monitorStream() => _value(_monitor);

  @override
  Stream<ChillerTelemetry> chillerStream() => _value(_chiller);

  @override
  Stream<DhTelemetry> dhStream() => _value(_dh);

  @override
  Stream<DhState> dhStateStream() => _value(_dhState);

  @override
  Stream<ChillerState> chillerStateStream() => _value(_chillerState);

  @override
  Stream<Heartbeat> heartbeatStream() => _value(_heartbeat);

  @override
  Stream<ControlParams> controlParamsStream() => _value(_controlParams);

  @override
  Stream<DhConfig> dhConfigStream() => _value(_dhConfig);

  @override
  Future<List<String>> discoverSystems() async => knownSystems;

  @override
  Future<bool> isKnownSystem(String systemId) async =>
      knownSystems.contains(systemId);

  @override
  Future<void> updateControlParams({
    double? comfortSetpointC,
    double? dewpointMarginC,
    double? weatherCoolTempC,
  }) async {
    lastControlParamsWrite = ControlParams(
      comfortSetpointC: comfortSetpointC ?? 24,
      dewpointMarginC: dewpointMarginC ?? 2,
      weatherCoolTempC: weatherCoolTempC ?? 28,
    );
  }

  @override
  Future<void> updateDhConfig({
    double? humiditySetpointPct,
    double? humidityDeadbandPct,
  }) async {
    lastDhConfigWrite = DhConfig(
      humiditySetpointPct: humiditySetpointPct ?? 55,
      humidityDeadbandPct: humidityDeadbandPct ?? 5,
    );
  }
}

class FakeTelemetryLogger extends TelemetryLogger {
  FakeTelemetryLogger() : super();

  final List<TelemetryPoint> _points = [];

  @override
  Future<List<TelemetryPoint>> load() async => List.unmodifiable(_points);

  @override
  Future<void> log(TelemetryPoint point) async {
    _points.add(point);
  }

  @override
  Future<List<TelemetryPoint>> loadWindow(Duration window) async {
    final cutoff = DateTime.now().subtract(window);
    return [
      for (final p in _points)
        if (!p.timestamp.isBefore(cutoff)) p,
    ];
  }

  @override
  Future<void> clear() async => _points.clear();
}

class FakeAlertService extends AlertService {
  FakeAlertService() : super();

  final List<Alert> _alerts = [];

  @override
  Future<List<Alert>> load() async => List.unmodifiable(_alerts);

  @override
  Future<void> addAlert(Alert alert) async {
    _alerts.insert(0, alert);
  }

  @override
  Future<void> clear() async => _alerts.clear();
}

class FakeNotificationService extends NotificationService {
  FakeNotificationService() : super();

  NotificationPrefs _prefs = const NotificationPrefs();

  @override
  Future<NotificationPrefs> load() async => _prefs;

  @override
  Future<void> save(NotificationPrefs prefs) async => _prefs = prefs;
}
