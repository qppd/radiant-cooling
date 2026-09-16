import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/models/telemetry.dart';
import 'package:radiant_cooling/widgets/device_status_strip.dart';

import 'fakes.dart';

void main() {
  testWidgets('shows all three devices', (tester) async {
    final firebase = FakeRadiantFirebase(
      heartbeat: const Heartbeat(
        online: true,
        deviceId: 'RADIANT-001',
        ts: 1786119829,
      ),
      chiller: const ChillerTelemetry(waterTempC: 15.0, ts: 1786119829),
      dh: const DhTelemetry(tempC: 25.0, humidityPct: 55, ts: 1786119829),
    );

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: DeviceStatusStrip(firebase: firebase),
        ),
      ),
    );
    await tester.pump();
    await tester.pump();

    expect(find.text('Monitor'), findsOneWidget);
    expect(find.text('Chiller'), findsOneWidget);
    expect(find.text('Dehumidifier'), findsOneWidget);
  });

  testWidgets('shows online/offline status based on data', (tester) async {
    final firebase = FakeRadiantFirebase(
      heartbeat: const Heartbeat(
        online: true,
        deviceId: 'RADIANT-001',
        ts: 1786119829,
      ),
      chiller: const ChillerTelemetry(waterTempC: 15.0, ts: 1786119829),
      dh: const DhTelemetry(),
    );

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: DeviceStatusStrip(firebase: firebase),
        ),
      ),
    );
    await tester.pump();
    await tester.pump();

    final onlineTexts = find.text('Online');
    final offlineTexts = find.text('Offline');
    expect(onlineTexts, findsNWidgets(2));
    expect(offlineTexts, findsOneWidget);
  });

  testWidgets('all offline when no data', (tester) async {
    final firebase = FakeRadiantFirebase();

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: DeviceStatusStrip(firebase: firebase),
        ),
      ),
    );
    await tester.pump();
    await tester.pump();

    expect(find.text('Offline'), findsNWidgets(3));
  });
}
