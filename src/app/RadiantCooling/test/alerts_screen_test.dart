import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/models/alert.dart';
import 'package:radiant_cooling/screens/alerts_screen.dart';

import 'fakes.dart';

void main() {
  testWidgets('shows empty state when no alerts', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: AlertsScreen(alertService: FakeAlertService()),
        ),
      ),
    );
    await tester.pump();
    await tester.pump();

    expect(find.text('No alerts'), findsOneWidget);
    expect(find.text('Your system is running smoothly.'), findsOneWidget);
  });

  testWidgets('shows alerts list', (tester) async {
    final service = FakeAlertService();
    await service.addAlert(Alert(
      type: AlertType.gatewayOffline,
      severity: AlertSeverity.critical,
      title: 'Gateway offline',
      message: 'Device RADIANT-001 is no longer responding.',
      timestamp: DateTime.now().subtract(const Duration(minutes: 5)),
    ));
    await service.addAlert(Alert(
      type: AlertType.condensationRisk,
      severity: AlertSeverity.warning,
      title: 'Condensation risk',
      message: 'Pipe temp below floor.',
      timestamp: DateTime.now().subtract(const Duration(hours: 1)),
    ));

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: AlertsScreen(alertService: service),
        ),
      ),
    );
    await tester.pump();
    await tester.pump();

    expect(find.text('Gateway offline'), findsOneWidget);
    expect(find.text('Condensation risk'), findsOneWidget);
    expect(find.text('Device RADIANT-001 is no longer responding.'), findsOneWidget);
  });

  testWidgets('filter chips filter alerts', (tester) async {
    final service = FakeAlertService();
    await service.addAlert(Alert(
      type: AlertType.gatewayOffline,
      severity: AlertSeverity.critical,
      title: 'Critical alert',
      message: 'msg',
      timestamp: DateTime.now(),
    ));
    await service.addAlert(Alert(
      type: AlertType.systemLinked,
      severity: AlertSeverity.info,
      title: 'Info alert',
      message: 'msg',
      timestamp: DateTime.now(),
    ));

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: AlertsScreen(alertService: service),
        ),
      ),
    );
    await tester.pump();
    await tester.pump();

    expect(find.text('Critical alert'), findsOneWidget);
    expect(find.text('Info alert'), findsOneWidget);

    await tester.tap(find.text('Critical'));
    await tester.pump();

    expect(find.text('Critical alert'), findsOneWidget);
    expect(find.text('Info alert'), findsNothing);

    await tester.tap(find.text('All'));
    await tester.pump();

    expect(find.text('Critical alert'), findsOneWidget);
    expect(find.text('Info alert'), findsOneWidget);
  });
}
