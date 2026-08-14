import 'dart:async';

import 'package:firebase_auth/firebase_auth.dart';
import 'package:radiant_cooling/models/telemetry.dart';
import 'package:radiant_cooling/services/auth_service.dart';
import 'package:radiant_cooling/services/radiant_firebase.dart';

/// Test double for [AuthService]: records calls and can throw on demand.
/// Built on the lazy-injection seam, so no Firebase plugin is touched.
class FakeAuthService extends AuthService {
  FakeAuthService() : super();

  final _authState = StreamController<User?>.broadcast();

  int signInCalls = 0;
  int signUpCalls = 0;
  String? lastSignInEmail;
  String? lastSignInPassword;
  String? lastSignUpEmail;
  String? lastSignUpPassword;

  /// When set, [signIn]/[signUp] throw this (e.g. a `FirebaseAuthException`).
  Object? signInError;
  Object? signUpError;

  @override
  Stream<User?> get authState => _authState.stream;

  @override
  Future<void> signIn(String email, String password) async {
    signInCalls++;
    // Mirror AuthService: emails are trimmed before being sent to Firebase.
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

/// Test double for [RadiantFirebase]. Each stream getter returns a fresh
/// single-value stream (so every listener — multiple `StreamBuilder`s or
/// a `stream.first` call — receives the value), and config writes are
/// recorded.
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

  /// Device registry used by [discoverSystems]/[isKnownSystem] (linking).
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
