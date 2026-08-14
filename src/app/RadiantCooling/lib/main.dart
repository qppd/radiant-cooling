import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';

import 'config/app_config.dart';
import 'screens/auth_screen.dart';
import 'screens/dashboard_screen.dart';
import 'screens/link_device_screen.dart';
import 'screens/settings_screen.dart';
import 'services/auth_service.dart';
import 'services/device_link.dart';
import 'services/radiant_firebase.dart';
import 'services/weather_key_store.dart';
import 'widgets/app_logo.dart';
import 'widgets/app_shell.dart';

/// Firebase options shared by every platform. Explicit options mean the app
/// works without a per-platform google-services.json — values live in the
/// git-ignored `AppConfig` (see `app_config.example.dart` for the template).
FirebaseOptions get _firebaseOptions => const FirebaseOptions(
      apiKey: AppConfig.firebaseApiKey,
      appId: AppConfig.firebaseAppId,
      messagingSenderId: AppConfig.firebaseMessagingSenderId,
      projectId: AppConfig.firebaseProjectId,
      authDomain: AppConfig.firebaseAuthDomain,
      databaseURL: AppConfig.firebaseDatabaseUrl,
      storageBucket: AppConfig.firebaseStorageBucket,
    );

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(options: _firebaseOptions);
  runApp(const RadiantCoolingApp());
}
class RadiantCoolingApp extends StatelessWidget {
  const RadiantCoolingApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Radiant Cooling',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.cyan),
        useMaterial3: true,
      ),
      home: const AuthGate(),
    );
  }
}

/// Shows the login/signup screen when signed out, the main shell when in.
class AuthGate extends StatefulWidget {
  const AuthGate({super.key});

  @override
  State<AuthGate> createState() => _AuthGateState();
}

class _AuthGateState extends State<AuthGate> {
  late final AuthService _auth;

  @override
  void initState() {
    super.initState();
    _auth = AuthService();
  }

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<User?>(
      stream: _auth.authState,
      builder: (context, snapshot) {
        if (snapshot.connectionState == ConnectionState.waiting) {
          return Scaffold(
            body: Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  const AppLogo(size: 112),
                  const SizedBox(height: 24),
                  const CircularProgressIndicator(),
                ],
              ),
            ),
          );
        }
        final user = snapshot.data;
        if (user == null) return AuthScreen(auth: _auth);
        return HomeShell(auth: _auth);
      },
    );
  }
}

/// Bottom-navigation shell: Dashboard (live data) and Settings.
class HomeShell extends StatefulWidget {
  const HomeShell({super.key, required this.auth});

  final AuthService auth;

  @override
  State<HomeShell> createState() => _HomeShellState();
}

class _HomeShellState extends State<HomeShell> {
  final _deviceLink = DeviceLink();
  final _keyStore = WeatherKeyStore();
  late final RadiantFirebase _firebase;

  String? _linkedId;
  String? _weatherKey;
  bool _loading = true;
  int _tabIndex = 0;

  @override
  void initState() {
    super.initState();
    _firebase = RadiantFirebase();
    _load();
  }

  Future<void> _load() async {
    final id = await _deviceLink.load();
    final key = await _keyStore.load();
    if (!mounted) return;
    setState(() {
      _linkedId = id;
      _weatherKey = key;
      _loading = false;
    });
    // The gateway keeps the key in RAM only, so re-deliver it whenever the
    // app starts (e.g. after a gateway reboot).
    if (id != null && key != null && key.isNotEmpty) {
      await _publishKey(key);
    }
  }

  Future<void> _publishKey(String key) async {
    if (_linkedId == null) return;
    try {
      await _firebase.publishWeatherKey(key);
    } catch (_) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text(
              'Could not deliver the WeatherAPI key — check the Firebase '
              'setup (google-services.json).',
            ),
          ),
        );
      }
    }
  }

  Future<void> _manageKey() async {
    final controller = TextEditingController(text: _weatherKey ?? '');
    final key = await showDialog<String>(
      context: context,
      builder: (context) => _KeyDialog(controller: controller),
    );
    if (key == null || key.isEmpty) return;
    await _keyStore.save(key);
    if (!mounted) return;
    setState(() => _weatherKey = key);
    await _publishKey(key);
  }

  Future<void> _linkSystem() async {
    // Listing the registry is best-effort: a permission/offline failure must
    // not block manual linking (the text field still works without chips).
    List<String> discovered = const [];
    try {
      discovered = await _firebase.discoverSystems();
    } catch (_) {}
    if (!mounted) return;
    setState(() {});

    final controller = TextEditingController(
      text: _linkedId ?? AppConfig.defaultSystemId,
    );
    final id = await showDialog<String>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Link system'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Text(
              'Enter the SYSTEM_ID configured on the gateway '
              '(SYSTEM_ID in Config.h). Found in the device registry:',
            ),
            const SizedBox(height: 12),
            TextField(
              controller: controller,
              decoration: const InputDecoration(
                labelText: 'System ID',
                hintText: 'e.g. RADIANT-001',
              ),
            ),
            if (discovered.isNotEmpty) ...[
              const SizedBox(height: 12),
              Wrap(
                spacing: 6,
                children: [
                  for (final id in discovered)
                    ActionChip(
                      label: Text(id),
                      onPressed: () => controller.text = id,
                    ),
                ],
              ),
            ],
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, controller.text.trim()),
            child: const Text('Link'),
          ),
        ],
      ),
    );

    if (id == null || id.isEmpty) return;

    // Validate against the device registry (written by the gateway) so the
    // user is told if the gateway is offline or the ID is wrong.
    final known = await _firebase.isKnownSystem(id);
    await _linkAndSave(id);
    if (!mounted) return;
    if (!known) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text(
            'System not found in the registry — is the gateway online?',
          ),
        ),
      );
    }
  }

  /// Save a linked system ID and re-deliver the WeatherAPI key to the
  /// gateway (the gateway keeps it in RAM only).
  Future<void> _linkAndSave(String id) async {
    await _deviceLink.save(id);
    if (!mounted) return;
    setState(() => _linkedId = id);
    if (_weatherKey != null && _weatherKey!.isNotEmpty) {
      await _publishKey(_weatherKey!);
    }
  }

  /// Called by the first-login [LinkDeviceScreen] after a successful link.
  void _onLinkedFromScreen(String id) {
    if (!mounted) return;
    setState(() => _linkedId = id);
    if (_weatherKey != null && _weatherKey!.isNotEmpty) {
      _publishKey(_weatherKey!);
    }
  }

  Future<void> _signOut() async {
    await widget.auth.signOut();
    if (!mounted) return;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(const SnackBar(content: Text('Signed out')));
  }

  @override
  Widget build(BuildContext context) {
    // First login / new account: force the device linking page before the
    // shell so the dashboard has a system to stream.
    if (_loading) {
      return const Scaffold(body: Center(child: CircularProgressIndicator()));
    }
    if (_linkedId == null) {
      return LinkDeviceScreen(
        firebase: _firebase,
        deviceLink: _deviceLink,
        onLinked: _onLinkedFromScreen,
      );
    }
    return AppShell(
      tabIndex: _tabIndex,
      onTabChanged: (i) => setState(() => _tabIndex = i),
      children: [
        DashboardScreen(firebase: _firebase, linkedId: _linkedId),
        SettingsScreen(
          firebase: _firebase,
          linkedId: _linkedId,
          weatherKey: _weatherKey,
          onLinkSystem: _linkSystem,
          onManageKey: _manageKey,
          onSignOut: _signOut,
        ),
      ],
    );
  }
}

class _KeyDialog extends StatefulWidget {
  const _KeyDialog({required this.controller});

  final TextEditingController controller;

  @override
  State<_KeyDialog> createState() => _KeyDialogState();
}

class _KeyDialogState extends State<_KeyDialog> {
  bool _obscure = true;

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('WeatherAPI key'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Text(
            'Enter your WeatherAPI.com key. It is stored on this phone and '
            'sent to the gateway via Firebase; the ESP32 uses it for its '
            'own weather requests.',
          ),
          const SizedBox(height: 12),
          TextField(
            controller: widget.controller,
            obscureText: _obscure,
            decoration: InputDecoration(
              labelText: 'API key',
              hintText: 'e.g. 1a2b3c4d5e6f...',
              suffixIcon: IconButton(
                icon: Icon(_obscure ? Icons.visibility : Icons.visibility_off),
                onPressed: () => setState(() => _obscure = !_obscure),
              ),
            ),
          ),
        ],
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
        FilledButton(
          onPressed: () =>
              Navigator.pop(context, widget.controller.text.trim()),
          child: const Text('Save'),
        ),
      ],
    );
  }
}
