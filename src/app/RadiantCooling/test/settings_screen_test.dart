import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/screens/settings_screen.dart';
import 'package:radiant_cooling/services/radiant_firebase.dart';

import 'fakes.dart';

void main() {
  Future<void> pumpSettings(
    WidgetTester tester,
    FakeRadiantFirebase firebase, {
    String? linkedId = 'RADIANT-001',
    String? weatherKey,
    VoidCallback? onLinkSystem,
    VoidCallback? onManageKey,
    VoidCallback? onSignOut,
  }) async {
    tester.view.physicalSize = const Size(800, 1800);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: SettingsScreen(
            firebase: firebase,
            linkedId: linkedId,
            weatherKey: weatherKey,
            onLinkSystem: onLinkSystem ?? () {},
            onManageKey: onManageKey ?? () {},
            onSignOut: onSignOut ?? () {},
          ),
        ),
      ),
    );
    await tester.pump();
  }

  testWidgets('shows the linked system, key status, and sign-out action',
      (tester) async {
    var linked = false;
    var keyManaged = false;
    var signedOut = false;
    final firebase = FakeRadiantFirebase(
      controlParams: const ControlParams(),
      dhConfig: const DhConfig(),
    );
    await pumpSettings(
      tester,
      firebase,
      linkedId: 'RADIANT-001',
      weatherKey: 'abc123',
      onLinkSystem: () => linked = true,
      onManageKey: () => keyManaged = true,
      onSignOut: () => signedOut = true,
    );

    expect(find.text('RADIANT-001'), findsOneWidget);
    expect(find.text('WeatherAPI key set'), findsOneWidget);
    expect(find.text('Change'), findsNWidgets(2)); // link + key
    expect(find.text('Sign out'), findsOneWidget);

    await tester.tap(find.text('Sign out'));
    expect(signedOut, isTrue);

    await tester.tap(find.text('Change').first);
    expect(linked, isTrue);

    await tester.tap(find.text('Change').last);
    expect(keyManaged, isTrue);
  });

  testWidgets('shows unlinked / no-key states with Link and Set key actions',
      (tester) async {
    var linked = false;
    var keyManaged = false;
    final firebase = FakeRadiantFirebase(
      controlParams: const ControlParams(),
      dhConfig: const DhConfig(),
    );
    await pumpSettings(
      tester,
      firebase,
      linkedId: null,
      weatherKey: null,
      onLinkSystem: () => linked = true,
      onManageKey: () => keyManaged = true,
    );

    expect(find.text('Not linked'), findsOneWidget);
    expect(find.text('No WeatherAPI key'), findsOneWidget);

    await tester.tap(find.text('Link'));
    expect(linked, isTrue);

    await tester.tap(find.text('Set key'));
    expect(keyManaged, isTrue);
  });

  testWidgets('shows current control params and dehumidifier target from the '
      'streams', (tester) async {
    final firebase = FakeRadiantFirebase(
      controlParams: const ControlParams(
        comfortSetpointC: 26.5,
        dewpointMarginC: 3,
        weatherCoolTempC: 31,
      ),
      dhConfig: const DhConfig(humiditySetpointPct: 60, humidityDeadbandPct: 8),
    );
    await pumpSettings(tester, firebase);

    expect(find.text('26.5 °C'), findsOneWidget);
    expect(find.text('3.0 °C'), findsOneWidget);
    expect(find.text('31.0 °C'), findsOneWidget);
    expect(find.text('60 %'), findsOneWidget);
    expect(find.text('8 %'), findsOneWidget);
  });

  testWidgets('editing control params saves to firebase and shows a toast',
      (tester) async {
    final firebase = FakeRadiantFirebase(
      controlParams: const ControlParams(),
      dhConfig: const DhConfig(),
    );
    await pumpSettings(tester, firebase);

    await tester.tap(find.widgetWithText(TextButton, 'Edit').first);
    await tester.pumpAndSettle();

    // Dialog opens with the three numeric fields.
    expect(find.text('Comfort setpoint (°C)'), findsOneWidget);
    await tester.enterText(find.byType(TextFormField).at(0), '27.0');
    await tester.tap(find.widgetWithText(FilledButton, 'Save'));
    await tester.pumpAndSettle();

    expect(firebase.lastControlParamsWrite?.comfortSetpointC, 27.0);
    expect(firebase.lastControlParamsWrite?.dewpointMarginC, 2.0);
    expect(firebase.lastControlParamsWrite?.weatherCoolTempC, 28.0);
    expect(find.text('Control parameters saved'), findsOneWidget);
  });

  testWidgets('the control-params dialog rejects out-of-range values',
      (tester) async {
    final firebase = FakeRadiantFirebase(
      controlParams: const ControlParams(),
      dhConfig: const DhConfig(),
    );
    await pumpSettings(tester, firebase);

    await tester.tap(find.widgetWithText(TextButton, 'Edit').first);
    await tester.pumpAndSettle();

    await tester.enterText(find.byType(TextFormField).at(0), '50');
    await tester.tap(find.widgetWithText(FilledButton, 'Save'));
    await tester.pump();

    expect(find.text('Between 16 and 32'), findsOneWidget);
    expect(firebase.lastControlParamsWrite, isNull);
  });

  testWidgets('editing the dehumidifier target saves dh config',
      (tester) async {
    final firebase = FakeRadiantFirebase(
      controlParams: const ControlParams(),
      dhConfig: const DhConfig(),
    );
    await pumpSettings(tester, firebase);

    await tester.tap(find.widgetWithText(TextButton, 'Edit').last);
    await tester.pumpAndSettle();

    expect(find.text('Humidity setpoint (%)'), findsOneWidget);
    await tester.enterText(find.byType(TextFormField).at(0), '60');
    await tester.tap(find.widgetWithText(FilledButton, 'Save'));
    await tester.pumpAndSettle();

    expect(firebase.lastDhConfigWrite?.humiditySetpointPct, 60.0);
    expect(firebase.lastDhConfigWrite?.humidityDeadbandPct, 5.0);
    expect(find.text('Dehumidifier target saved'), findsOneWidget);
  });
}
