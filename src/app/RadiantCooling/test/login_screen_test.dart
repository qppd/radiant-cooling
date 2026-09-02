import 'package:firebase_auth/firebase_auth.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/screens/login_screen.dart';

import 'fakes.dart';

void main() {
  Future<void> pumpLogin(WidgetTester tester, FakeAuthService auth) async {
    await tester.pumpWidget(MaterialApp(home: LoginScreen(auth: auth)));
  }

  testWidgets('shows the login form with email and password fields',
      (tester) async {
    await pumpLogin(tester, FakeAuthService());

    expect(find.text('Sign in to control your system'), findsOneWidget);
    expect(find.text('Email'), findsOneWidget);
    expect(find.text('Password'), findsOneWidget);
    expect(find.widgetWithText(FilledButton, 'Sign in'), findsOneWidget);
    expect(find.textContaining("Don't have an account"), findsOneWidget);
  });

  testWidgets('validates empty and malformed input', (tester) async {
    await pumpLogin(tester, FakeAuthService());

    // Submit with empty fields.
    await tester.tap(find.widgetWithText(FilledButton, 'Sign in'));
    await tester.pump();
    expect(find.text('Enter your email'), findsOneWidget);
    expect(find.text('Enter a password'), findsOneWidget);

    // Invalid email format.
    await tester.enterText(find.byType(TextFormField).at(0), 'not-an-email');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign in'));
    await tester.pump();
    expect(find.text('Enter a valid email address'), findsOneWidget);
  });

  testWidgets('successful sign-in calls the service with a trimmed email',
      (tester) async {
    final auth = FakeAuthService();
    await pumpLogin(tester, auth);

    await tester.enterText(
      find.byType(TextFormField).at(0),
      '  user@example.com  ',
    );
    await tester.enterText(find.byType(TextFormField).at(1), 'secret123');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign in'));
    await tester.pump();

    expect(auth.signInCalls, 1);
    expect(auth.lastSignInEmail, 'user@example.com');
    expect(auth.lastSignInPassword, 'secret123');
  });

  testWidgets('maps a Firebase auth error to a friendly message',
      (tester) async {
    final auth = FakeAuthService()
      ..signInError = FirebaseAuthException(code: 'wrong-password');
    await pumpLogin(tester, auth);

    await tester.enterText(find.byType(TextFormField).at(0), 'user@example.com');
    await tester.enterText(find.byType(TextFormField).at(1), 'nope');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign in'));
    await tester.pump();
    await tester.pump();

    expect(find.text('Incorrect password.'), findsOneWidget);
  });

  testWidgets('password visibility toggle works', (tester) async {
    await pumpLogin(tester, FakeAuthService());

    // Initially obscured — check the underlying TextField.
    final passwordField = tester.widget<TextField>(
      find.descendant(
        of: find.byType(TextFormField).at(1),
        matching: find.byType(TextField),
      ),
    );
    expect(passwordField.obscureText, isTrue);

    // Tap the visibility toggle.
    await tester.tap(
      find.descendant(
        of: find.byType(TextFormField).at(1),
        matching: find.byType(IconButton),
      ),
    );
    await tester.pump();

    final toggled = tester.widget<TextField>(
      find.descendant(
        of: find.byType(TextFormField).at(1),
        matching: find.byType(TextField),
      ),
    );
    expect(toggled.obscureText, isFalse);
  });

  testWidgets('does not call signUp (login only)', (tester) async {
    final auth = FakeAuthService();
    await pumpLogin(tester, auth);

    await tester.enterText(find.byType(TextFormField).at(0), 'user@example.com');
    await tester.enterText(find.byType(TextFormField).at(1), 'secret123');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign in'));
    await tester.pump();

    expect(auth.signUpCalls, 0);
    expect(auth.signInCalls, 1);
  });
}
