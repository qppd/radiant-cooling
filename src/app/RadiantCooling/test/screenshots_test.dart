// Golden-capture harness for the app's screenshots (docs/screenshots/).
//
// By default these tests are SKIPPED (they only render app chrome with real
// fonts and compare goldens, which is meaningless without the goldens). To
// (re)generate the screenshots run:
//
//   flutter test --dart-define=GEN_SCREENSHOTS=true --update-goldens \
//     test/screenshots_test.dart
//
// PNGs are written to docs/screenshots/ (git-ignored).
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/models/telemetry.dart';
import 'package:radiant_cooling/screens/auth_screen.dart';
import 'package:radiant_cooling/screens/dashboard_screen.dart';
import 'package:radiant_cooling/screens/link_device_screen.dart';
import 'package:radiant_cooling/screens/settings_screen.dart';
import 'package:radiant_cooling/services/device_link.dart';
import 'package:radiant_cooling/services/radiant_firebase.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'fakes.dart';

const _gen = bool.fromEnvironment('GEN_SCREENSHOTS');

const _phoneSize = Size(390, 844); // logical pixels

/// Load real fonts so text renders as text (not the blocky test font).
Future<void> _loadRealFonts() async {
  const dir = 'C:/Windows/Fonts';
  Future<ByteData> bytes(String file) async =>
      (await File('$dir/$file').readAsBytes()).buffer.asByteData();

  if (!File('$dir/segoeui.ttf').existsSync()) return; // non-Windows: fall back
  final loader = FontLoader('Roboto')
    ..addFont(bytes('segoeui.ttf'));
  await loader.load();
}

ThemeData _theme() => ThemeData(
  colorScheme: ColorScheme.fromSeed(seedColor: Colors.cyan),
  useMaterial3: true,
);

void _setPhone(WidgetTester tester) {
  tester.view.physicalSize = _phoneSize;
  tester.view.devicePixelRatio = 1.0;
  addTearDown(tester.view.reset);
}

Future<void> _capture(WidgetTester tester, String name) async {
  await tester.pumpAndSettle();
  // Resolved relative to this test file (src/app/RadiantCooling/test/):
  // ../../../../ = repo root.
  await expectLater(
    find.byType(MaterialApp),
    matchesGoldenFile('../../../../docs/screenshots/$name.png'),
  );
}

Future<void> _pumpApp(WidgetTester tester, Widget home) async {
  await tester.pumpWidget(
    MaterialApp(theme: _theme(), home: home, debugShowCheckedModeBanner: false),
  );
  await tester.pump();
}

void main() {
  setUpAll(_loadRealFonts);

  testWidgets('screenshots: auth login', (tester) async {
    _setPhone(tester);
    await _pumpApp(tester, AuthScreen(auth: FakeAuthService()));
    await _capture(tester, 'auth_login');
  }, skip: !_gen);

  testWidgets('screenshots: auth sign-up', (tester) async {
    _setPhone(tester);
    final auth = FakeAuthService();
    await _pumpApp(tester, AuthScreen(auth: auth));
    await tester.tap(
      find.descendant(
        of: find.byType(SegmentedButton<bool>),
        matching: find.text('Sign up'),
      ),
    );
    await tester.pumpAndSettle();
    await _capture(tester, 'auth_signup');
  }, skip: !_gen);

  testWidgets('screenshots: link device', (tester) async {
    _setPhone(tester);
    SharedPreferences.setMockInitialValues({});
    final firebase = FakeRadiantFirebase()..knownSystems = [
      'RADIANT-001',
      'RADIANT-002',
    ];
    await _pumpApp(
      tester,
      LinkDeviceScreen(
        firebase: firebase,
        deviceLink: DeviceLink(),
        onLinked: (_) {},
      ),
    );
    await _capture(tester, 'link_device');
  }, skip: !_gen);

  testWidgets('screenshots: dashboard', (tester) async {
    _setPhone(tester);
    final firebase = FakeRadiantFirebase(
      heartbeat: Heartbeat(
        online: true,
        deviceId: 'RADIANT-001',
        ts: 1786695300,
      ),
      monitor: const MonitorTelemetry(
        supplyC: 7.2,
        returnC: 9.8,
        coldestPipeC: 7.2,
        deltaTC: 2.6,
        tempsC: [7.2, 9.8, 12.4, 14.1, 15.0, 16.3],
        outdoorTempC: 29.5,
        outdoorDewPointC: 24.9,
        outdoorHumidityPct: 74,
        dewPointC: 24.9,
        waterFloorC: 26.9,
      ),
      chiller: const ChillerTelemetry(waterTempC: 6.8),
      dh: const DhTelemetry(tempC: 27.4, humidityPct: 68),
      dhState: const DhState(on: true),
      chillerState: const ChillerState(pump1On: true, pump2On: true),
      controlParams: const ControlParams(),
    );
    await _pumpApp(
      tester,
      Scaffold(body: DashboardScreen(firebase: firebase, linkedId: 'RADIANT-001')),
    );
    await _capture(tester, 'dashboard');
  }, skip: !_gen);

  testWidgets('screenshots: settings', (tester) async {
    _setPhone(tester);
    final firebase = FakeRadiantFirebase(
      controlParams: const ControlParams(),
      dhConfig: const DhConfig(),
    );
    await _pumpApp(
      tester,
      Scaffold(
        body: SettingsScreen(
          firebase: firebase,
          linkedId: 'RADIANT-001',
          weatherKey: 'set',
          onLinkSystem: () {},
          onManageKey: () {},
          onSignOut: () {},
        ),
      ),
    );
    await _capture(tester, 'settings');
  }, skip: !_gen);
}
