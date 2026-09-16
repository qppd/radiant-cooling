import 'package:firebase_auth/firebase_auth.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/screens/register_screen.dart';

import 'fakes.dart';

void main() {
  Future<void> pumpRegister(WidgetTester tester, FakeAuthService auth) async {
    await tester.pumpWidget(MaterialApp(home: RegisterScreen(auth: auth)));
  }

  testWidgets('shows the registration form with all fields', (tester) async {
    await pumpRegister(tester, FakeAuthService());

    expect(find.text('Create your account'), findsOneWidget);
    expect(find.text('Email'), findsOneWidget);
    expect(find.text('Password'), findsOneWidget);
    expect(find.text('Confirm password'), findsOneWidget);
    expect(find.widgetWithText(FilledButton, 'Sign up'), findsOneWidget);
    expect(find.textContaining('Already have an account'), findsOneWidget);
  });

  testWidgets('validates empty fields', (tester) async {
    await pumpRegister(tester, FakeAuthService());

    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();
    expect(find.text('Enter your email'), findsOneWidget);
    expect(find.text('Enter a password'), findsOneWidget);
  });

  testWidgets('validates short password (min 6 chars)', (tester) async {
    await pumpRegister(tester, FakeAuthService());

    await tester.enterText(find.byType(TextFormField).at(0), 'a@b.co');
    await tester.enterText(find.byType(TextFormField).at(1), '123');
    await tester.enterText(find.byType(TextFormField).at(2), '123');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();

    expect(find.text('Use at least 6 characters'), findsOneWidget);
  });

  testWidgets('validates mismatched confirm password', (tester) async {
    await pumpRegister(tester, FakeAuthService());

    await tester.enterText(find.byType(TextFormField).at(0), 'a@b.co');
    await tester.enterText(find.byType(TextFormField).at(1), '123456');
    await tester.enterText(find.byType(TextFormField).at(2), '654321');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();

    expect(find.text('Passwords do not match'), findsOneWidget);
  });

  testWidgets('successful sign-up calls the service with a trimmed email',
      (tester) async {
    final auth = FakeAuthService();
    await pumpRegister(tester, auth);

    await tester.enterText(
      find.byType(TextFormField).at(0),
      ' new@example.com ',
    );
    await tester.enterText(find.byType(TextFormField).at(1), '123456');
    await tester.enterText(find.byType(TextFormField).at(2), '123456');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();

    expect(auth.signUpCalls, 1);
    expect(auth.lastSignUpEmail, 'new@example.com');
    expect(auth.lastSignUpPassword, '123456');
  });

  testWidgets('maps Firebase email-already-in-use error', (tester) async {
    final auth = FakeAuthService()
      ..signUpError = FirebaseAuthException(code: 'email-already-in-use');
    await pumpRegister(tester, auth);

    await tester.enterText(find.byType(TextFormField).at(0), 'user@example.com');
    await tester.enterText(find.byType(TextFormField).at(1), '123456');
    await tester.enterText(find.byType(TextFormField).at(2), '123456');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();
    await tester.pump();

    expect(
      find.text('An account already exists for this email.'),
      findsOneWidget,
    );
  });

  testWidgets('maps Firebase weak-password error', (tester) async {
    final auth = FakeAuthService()
      ..signUpError = FirebaseAuthException(code: 'weak-password');
    await pumpRegister(tester, auth);

    await tester.enterText(find.byType(TextFormField).at(0), 'user@example.com');
    await tester.enterText(find.byType(TextFormField).at(1), '123456');
    await tester.enterText(find.byType(TextFormField).at(2), '123456');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();
    await tester.pump();

    expect(
      find.text('Password is too weak (min 6 characters).'),
      findsOneWidget,
    );
  });

  testWidgets('does not call signIn (register only)', (tester) async {
    final auth = FakeAuthService();
    await pumpRegister(tester, auth);

    await tester.enterText(find.byType(TextFormField).at(0), 'user@example.com');
    await tester.enterText(find.byType(TextFormField).at(1), '123456');
    await tester.enterText(find.byType(TextFormField).at(2), '123456');
    await tester.tap(find.widgetWithText(FilledButton, 'Sign up'));
    await tester.pump();

    expect(auth.signInCalls, 0);
    expect(auth.signUpCalls, 1);
  });

  testWidgets('password visibility toggle works', (tester) async {
    await pumpRegister(tester, FakeAuthService());

    final passwordField = tester.widget<TextField>(
      find.descendant(
        of: find.byType(TextFormField).at(1),
        matching: find.byType(TextField),
      ),
    );
    expect(passwordField.obscureText, isTrue);

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


}
