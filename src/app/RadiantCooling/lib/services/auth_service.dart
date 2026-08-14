import 'package:firebase_auth/firebase_auth.dart';

/// Email/password authentication for the app.
///
/// Any authenticated user may read telemetry/state/config/heartbeat and
/// write `config/**` (see `docs/firebase-security-rules.json`). The gateway
/// uses its own dedicated email/password account — this service is only for
/// the person using the app.
class AuthService {
  AuthService({FirebaseAuth? auth}) : _auth = auth;

  /// Injected instance, resolved to the platform one lazily so widget tests
  /// can subclass [AuthService] without the Firebase plugin.
  final FirebaseAuth? _auth;

  FirebaseAuth get auth => _auth ?? FirebaseAuth.instance;

  /// Emits the signed-in user (null when signed out).
  Stream<User?> get authState => auth.authStateChanges();

  Future<void> signIn(String email, String password) async {
    await auth.signInWithEmailAndPassword(
      email: email.trim(),
      password: password,
    );
  }

  Future<void> signUp(String email, String password) async {
    await auth.createUserWithEmailAndPassword(
      email: email.trim(),
      password: password,
    );
  }

  Future<void> signOut() => auth.signOut();
}
