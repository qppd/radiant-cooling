import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/models/telemetry.dart';
import 'package:radiant_cooling/screens/dashboard_screen.dart';

import 'fakes.dart';

void main() {
  Future<void> pumpDashboard(
    WidgetTester tester,
    FakeRadiantFirebase firebase, {
    String? linkedId = 'RADIANT-001',
  }) async {
    // Tall surface so the whole ListView is laid out (footer included).
    tester.view.physicalSize = const Size(800, 2400);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: DashboardScreen(firebase: firebase, linkedId: linkedId),
        ),
      ),
    );
    // Nested StreamBuilders resolve over a couple of frames.
    await tester.pump();
    await tester.pump();
  }

  testWidgets('shows the not-linked view when no system is linked',
      (tester) async {
    await pumpDashboard(tester, FakeRadiantFirebase(), linkedId: null);

    expect(find.text('No system linked yet'), findsOneWidget);
  });

  testWidgets('renders live telemetry, control state, and safety values',
      (tester) async {
    final firebase = FakeRadiantFirebase(
      heartbeat: const Heartbeat(
        online: true,
        deviceId: 'RADIANT-001',
        ts: 1786119829,
      ),
      monitor: const MonitorTelemetry(
        supplyC: 16.2,
        returnC: 16.8,
        coldestPipeC: 16.2,
        deltaTC: 0.6,
        tempsC: [16.2, 16.8, 17.1, 22.4],
        outdoorTempC: 31.2,
        outdoorDewPointC: 24.5,
        outdoorHumidityPct: 66,
        dewPointC: 18.0,
        waterFloorC: 20.0,
      ),
      chiller: const ChillerTelemetry(waterTempC: 12.4),
      dh: const DhTelemetry(tempC: 26.0, humidityPct: 55),
      dhState: const DhState(on: true),
      chillerState: const ChillerState(pump1On: true, pump2On: false),
    );
    await pumpDashboard(tester, firebase);

    // Gateway status.
    expect(find.text('Gateway online'), findsOneWidget);
    expect(find.textContaining('System RADIANT-001'), findsOneWidget);

    // Outdoor weather.
    expect(find.text('31.2 °C'), findsOneWidget);
    expect(find.text('24.5 °C'), findsOneWidget);
    expect(find.text('66 %'), findsOneWidget);

    // Cooling loop (supply appears twice: card + pipe sensor list).
    expect(find.text('16.2 °C'), findsWidgets);
    expect(find.text('16.8 °C'), findsWidgets);
    expect(find.text('0.6 °C'), findsOneWidget);
    expect(find.text('12.4 °C'), findsOneWidget); // tank

    // Pipe sensors.
    expect(find.text('17.1 °C'), findsOneWidget);
    expect(find.text('22.4 °C'), findsOneWidget);

    // Control section: pump 1 + dehumidifier ON, pump 2 OFF.
    expect(find.text('Chiller pump 1'), findsOneWidget);
    expect(find.text('Chiller pump 2'), findsOneWidget);
    expect(find.text('Dehumidifier'), findsOneWidget);
    expect(find.text('ON'), findsNWidgets(2));
    expect(find.text('OFF'), findsOneWidget);

    // Indoor climate.
    expect(find.text('26.0 °C'), findsOneWidget);
    expect(find.text('55 %'), findsOneWidget);

    // Condensation safety (values computed by the gateway).
    expect(find.text('18.0 °C'), findsOneWidget);
    expect(find.text('20.0 °C'), findsOneWidget);

    // Footer.
    expect(find.textContaining('device RADIANT-001'), findsOneWidget);
  });

  testWidgets('renders an offline gateway and empty-sensor placeholders',
      (tester) async {
    final firebase = FakeRadiantFirebase(
      heartbeat: const Heartbeat(online: false, deviceId: 'RADIANT-001'),
      monitor: const MonitorTelemetry(),
      chiller: const ChillerTelemetry(),
      dh: const DhTelemetry(),
      dhState: const DhState(),
      chillerState: const ChillerState(),
    );
    await pumpDashboard(tester, firebase);

    expect(find.text('Gateway offline'), findsOneWidget);
    expect(find.text('No pipe readings yet'), findsOneWidget);
    // Every metric falls back to an em dash.
    expect(find.text('—'), findsWidgets);
  });
}
