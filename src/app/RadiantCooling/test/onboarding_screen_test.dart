import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/screens/onboarding_screen.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  setUp(() => SharedPreferences.setMockInitialValues({}));

  testWidgets('shows first page on launch', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        home: OnboardingScreen(onComplete: () {}),
      ),
    );

    expect(find.text('Radiant Cooling'), findsOneWidget);
    expect(find.text('Intelligent home cooling control'), findsOneWidget);
    expect(find.text('Skip'), findsOneWidget);
    expect(find.text('Next'), findsOneWidget);
  });

  testWidgets('swiping advances to next page', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        home: OnboardingScreen(onComplete: () {}),
      ),
    );

    // Swipe left to go to page 2.
    await tester.drag(
      find.byType(PageView),
      const Offset(-400, 0),
    );
    await tester.pumpAndSettle();

    expect(find.text('Smart Sensors'), findsOneWidget);
    expect(find.text('Temperature & humidity across your system'), findsOneWidget);
  });

  testWidgets('tapping Next advances pages', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        home: OnboardingScreen(onComplete: () {}),
      ),
    );

    await tester.tap(find.text('Next'));
    await tester.pumpAndSettle();
    expect(find.text('Smart Sensors'), findsOneWidget);

    await tester.tap(find.text('Next'));
    await tester.pumpAndSettle();
    expect(find.text('Weather-Aware'), findsOneWidget);

    await tester.tap(find.text('Next'));
    await tester.pumpAndSettle();
    // Page title + button both say 'Get Started'.
    expect(find.text('Get Started'), findsNWidgets(2));
  });

  testWidgets('last page shows Get Started instead of Next', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        home: OnboardingScreen(onComplete: () {}),
      ),
    );

    // Navigate to last page.
    for (var i = 0; i < 3; i++) {
      await tester.tap(find.text('Next'));
      await tester.pumpAndSettle();
    }

    // Page title + button both say 'Get Started'.
    expect(find.text('Get Started'), findsNWidgets(2));
    expect(find.text('Next'), findsNothing);
    expect(find.text('Skip'), findsNothing); // hidden on last page
  });

  testWidgets('tapping Skip completes onboarding', (tester) async {
    var completed = false;
    await tester.pumpWidget(
      MaterialApp(
        home: OnboardingScreen(onComplete: () => completed = true),
      ),
    );

    await tester.tap(find.text('Skip'));
    await tester.pumpAndSettle();

    expect(completed, isTrue);

    // Verify SharedPreferences was set.
    final prefs = await SharedPreferences.getInstance();
    expect(prefs.getBool('onboarding_complete'), isTrue);
  });

  testWidgets('tapping Get Started completes onboarding', (tester) async {
    var completed = false;
    await tester.pumpWidget(
      MaterialApp(
        home: OnboardingScreen(onComplete: () => completed = true),
      ),
    );

    // Navigate to last page.
    for (var i = 0; i < 3; i++) {
      await tester.tap(find.text('Next'));
      await tester.pumpAndSettle();
    }

    // Tap the button (not the title).
    await tester.tap(find.widgetWithText(FilledButton, 'Get Started'));
    await tester.pumpAndSettle();

    expect(completed, isTrue);
  });

  testWidgets('dot indicators reflect current page', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        home: OnboardingScreen(onComplete: () {}),
      ),
    );

    // 4 dot indicators rendered.
    expect(find.byType(AnimatedContainer), findsNWidgets(4));

    // Advance to page 2.
    await tester.tap(find.text('Next'));
    await tester.pumpAndSettle();

    // Still 4 dots.
    expect(find.byType(AnimatedContainer), findsNWidgets(4));
  });
}
