import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/screens/trends_screen.dart';
import 'package:radiant_cooling/services/telemetry_logger.dart';

import 'fakes.dart';

void main() {
  testWidgets('shows empty state when no data', (tester) async {
    tester.view.physicalSize = const Size(800, 1600);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: TrendsScreen(
            firebase: FakeRadiantFirebase(),
            logger: FakeTelemetryLogger(),
          ),
        ),
      ),
    );
    await tester.pump();
    await tester.pump();

    expect(find.text('No data yet'), findsOneWidget);
  });

  testWidgets('shows charts when data exists', (tester) async {
    tester.view.physicalSize = const Size(800, 3000);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    final logger = FakeTelemetryLogger();
    final now = DateTime.now();
    for (var i = 0; i < 10; i++) {
      await logger.log(TelemetryPoint(
        timestamp: now.subtract(Duration(minutes: i * 5)),
        supplyC: 15.0 + i * 0.5,
        returnC: 16.0 + i * 0.3,
        tankTempC: 14.0 + i * 0.2,
        coldestPipeC: 13.0 + i * 0.1,
        indoorTempC: 25.0 + i * 0.1,
        indoorHumidityPct: 55.0 - i * 0.5,
        outdoorTempC: 30.0 + i * 0.2,
        outdoorDewPointC: 22.0 + i * 0.1,
      ));
    }

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: TrendsScreen(
            firebase: FakeRadiantFirebase(),
            logger: logger,
          ),
        ),
      ),
    );
    await tester.pump();
    await tester.pump();

    // Charts should render.
    expect(find.text('Pipe temperatures'), findsOneWidget);
    expect(find.text('Indoor climate'), findsOneWidget);
    expect(find.text('Outdoor weather'), findsOneWidget);
    expect(find.byType(LineChart), findsWidgets);

    // Summary stats.
    expect(find.text('Supply temperature summary'), findsOneWidget);
    expect(find.text('Points'), findsOneWidget);
  });

  testWidgets('period selector changes window', (tester) async {
    tester.view.physicalSize = const Size(800, 1600);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    final logger = FakeTelemetryLogger();
    final now = DateTime.now();
    // Add one point 10 minutes ago.
    await logger.log(TelemetryPoint(
      timestamp: now.subtract(const Duration(minutes: 10)),
      supplyC: 15.0,
    ));

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: TrendsScreen(
            firebase: FakeRadiantFirebase(),
            logger: logger,
          ),
        ),
      ),
    );
    await tester.pump();
    await tester.pump();

    // Default 6h window shows the point.
    expect(find.text('Points'), findsOneWidget);

    // Switch to 1h window.
    await tester.tap(find.text('1h'));
    await tester.pump();
    await tester.pump();

    // Point is within 1h, still visible.
    expect(find.text('Points'), findsOneWidget);
  });
}
