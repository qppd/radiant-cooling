import 'package:firebase_auth/firebase_auth.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/screens/auth_screen.dart';

import 'fakes.dart';

void main() {
  Future<void> pumpAuth(WidgetTester tester, FakeAuthService auth) async {
    await tester.pumpWidget(MaterialApp(home: AuthScreen(auth: auth)));
  }

  /// Taps the Login/Sign up segmented control.
  Future<void> switchMode(WidgetTester tester, String label) async {
    await tester.tap(
      find.descendant(
        of: find.byType(SegmentedButton<bool>),
        matching: find.text(label),
      ),
    );
    await tester.pump();
  }

  testWidgets('shows the login form by default', (tester) async {
    await pumpAuth(tester, FakeAuthService());

    expect(find.text('Sign in to control your system'), findsOneWidget);
    expect(find.text('Email'), findsOneWidget);
    expect(find.text('Password'), findsOneWidget);
    expect(find.text('Confirm password'), findsNothing);
    expect(find.widgetWithText(FilledButton, 'Sign in'), findsOneWidget);
  });

  testWidgets('validates empty and malformed input', (tester) async {
    await pumpAuth(tester, FakeAuthService());

    await tester.tap(find.widgetWithText(FilledButton, 'Sign in'));
    await tester.pump();
    expect(find.text('Enter your email'), findsOneWidget);
    expect(find.text('Enter a password'), findsOneWidget);

    await tester.enterText(find.byType(TextFormField).at(0), 'not-an-email');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign in'));
    await tester.pump();
    expect(find.text('Enter a valid email address'), findsOneWidget);
  });

  testWidgets('sign-up mode adds a confirm field and validates it',
      (tester) async {
    final auth = FakeAuthService();
    await pumpAuth(tester, auth);

    await switchMode(tester, 'Sign up');
    expect(find.text('Create your account'), findsOneWidget);
    expect(find.text('Confirm password'), findsOneWidget);

    // Short password.
    await tester.enterText(find.byType(TextFormField).at(0), 'a@b.co');
    await tester.enterText(find.byType(TextFormField).at(1), '123');
    await tester.enterText(find.byType(TextFormField).at(2), '123');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();
    expect(find.text('Use at least 6 characters'), findsOneWidget);
    expect(auth.signUpCalls, 0);

    // Mismatched confirmation.
    await tester.enterText(find.byType(TextFormField).at(1), '123456');
    await tester.enterText(find.byType(TextFormField).at(2), '654321');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();
    expect(find.text('Passwords do not match'), findsOneWidget);
    expect(auth.signUpCalls, 0);
  });

  testWidgets('successful sign-in calls the service with a trimmed email',
      (tester) async {
    final auth = FakeAuthService();
    await pumpAuth(tester, auth);

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

  testWidgets('successful sign-up calls the service with a trimmed email',
      (tester) async {
    final auth = FakeAuthService();
    await pumpAuth(tester, auth);

    await switchMode(tester, 'Sign up');
    await tester.enterText(find.byType(TextFormField).at(0), ' new@example.com ');
    await tester.enterText(find.byType(TextFormField).at(1), '123456');
    await tester.enterText(find.byType(TextFormField).at(2), '123456');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();

    expect(auth.signUpCalls, 1);
    expect(auth.lastSignUpEmail, 'new@example.com');
    expect(auth.lastSignUpPassword, '123456');
  });

  testWidgets('maps a Firebase auth error to a friendly message',
      (tester) async {
    final auth = FakeAuthService()
      ..signInError = FirebaseAuthException(code: 'wrong-password');
    await pumpAuth(tester, auth);

    await tester.enterText(find.byType(TextFormField).at(0), 'user@example.com');
    await tester.enterText(find.byType(TextFormField).at(1), 'nope');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign in'));
    await tester.pump();
    await tester.pump();

    expect(find.text('Incorrect password.'), findsOneWidget);
  });

  testWidgets('sign-up errors are surfaced too', (tester) async {
    final auth = FakeAuthService()
      ..signUpError = FirebaseAuthException(code: 'email-already-in-use');
    await pumpAuth(tester, auth);

    await switchMode(tester, 'Sign up');
    await tester.enterText(find.byType(TextFormField).at(0), 'user@example.com');
    await tester.enterText(find.byType(TextFormField).at(1), '123456');
    await tester.enterText(find.byType(TextFormField).at(2), '123456');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();
    await tester.pump();

    expect(find.text('An account already exists for this email.'), findsOneWidget);
  });
}
