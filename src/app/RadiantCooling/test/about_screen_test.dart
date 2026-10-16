import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/models/telemetry.dart';
import 'package:radiant_cooling/screens/about_screen.dart';

import 'fakes.dart';

void main() {
  testWidgets('shows the system, hardware and technology but no credits card',
      (tester) async {
    tester.view.physicalSize = const Size(800, 1800);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    await tester.pumpWidget(
      MaterialApp(
        home: AboutScreen(
          firebase: FakeRadiantFirebase(
            heartbeat: const Heartbeat(online: true, deviceId: 'RADIANT-001'),
          ),
          linkedId: 'RADIANT-001',
        ),
      ),
    );
    await tester.pump();

    // The credits card was removed from the About page.
    expect(find.text('Credits'), findsNothing);
    expect(find.textContaining('Sajed'), findsNothing);
    expect(find.text('Building intelligent solutions'), findsNothing);

    // The rest of the page is untouched.
    expect(find.text('Linked system'), findsOneWidget);
    expect(find.text('Hardware'), findsOneWidget);
    expect(find.text('Technology'), findsOneWidget);
    expect(find.text('MIT License'), findsOneWidget);
  });

  testWidgets('still renders without a linked system', (tester) async {
    tester.view.physicalSize = const Size(800, 1800);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.reset);

    await tester.pumpWidget(
      MaterialApp(
        home: AboutScreen(firebase: FakeRadiantFirebase(), linkedId: null),
      ),
    );
    await tester.pump();

    expect(find.text('Linked system'), findsNothing);
    expect(find.text('Hardware'), findsOneWidget);
    expect(find.text('Credits'), findsNothing);
  });
}
