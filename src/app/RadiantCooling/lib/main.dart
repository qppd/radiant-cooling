import 'dart:async';

import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';

import 'config/app_config.dart';
import 'screens/about_screen.dart';
import 'screens/alerts_screen.dart';
import 'screens/dashboard_screen.dart';
import 'screens/link_device_screen.dart';
import 'screens/login_screen.dart';
import 'screens/notification_settings_screen.dart';
import 'screens/onboarding_screen.dart';
import 'screens/settings_screen.dart';
import 'screens/trends_screen.dart';
import 'models/telemetry.dart';
import 'services/alert_service.dart';
import 'services/auth_service.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'services/device_link.dart';
import 'services/device_watchdog.dart';
import 'services/notification_service.dart';
import 'services/radiant_firebase.dart';
import 'services/telemetry_logger.dart';
import 'services/weather_key_store.dart';
import 'widgets/app_logo.dart';
import 'widgets/app_shell.dart';

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
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.cyan),
        useMaterial3: true,
      ),
      home: const AppEntry(),
    );
  }
}

class AppEntry extends StatefulWidget {
  const AppEntry({super.key});

  @override
  State<AppEntry> createState() => _AppEntryState();
}

class _AppEntryState extends State<AppEntry> {
  bool _checked = false;
  bool _onboardingComplete = false;

  @override
  void initState() {
    super.initState();
    _check();
  }

  Future<void> _check() async {
    final prefs = await SharedPreferences.getInstance();
    final complete = prefs.getBool('onboarding_complete') ?? false;
    if (!mounted) return;
    setState(() {
      _onboardingComplete = complete;
      _checked = true;
    });
  }

  @override
  Widget build(BuildContext context) {
    if (!_checked) {
      return const Scaffold(
        body: Center(child: CircularProgressIndicator()),
      );
    }
    if (!_onboardingComplete) {
      return OnboardingScreen(
        onComplete: () {
          setState(() => _onboardingComplete = true);
        },
      );
    }
    return const AuthGate();
  }
}

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
        if (user == null) return LoginScreen(auth: _auth);
        return HomeShell(auth: _auth);
      },
    );
  }
}

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
  final _telemetryLogger = TelemetryLogger();
  final _alertService = AlertService();
  final _notificationService = NotificationService();

  // Marks a device registry node offline when that device stops reporting.
  DeviceWatchdog? _watchdog;

  final List<StreamSubscription<dynamic>> _subs = [];

  String? _linkedId;
  String? _weatherKey;
  bool _loading = true;
  int _tabIndex = 0;
  int _unreadAlertCount = 0;

  // Latest readings from each stream (they arrive independently, on their
  // own ~30 s cycles) merged into one logged point.
  MonitorTelemetry? _lastMonitor;
  ChillerTelemetry? _lastChiller;
  DhTelemetry? _lastDh;
  DateTime? _lastLogAt;
  Timer? _logDebounce;

  /// Minimum spacing between logged points; coalesces the near-simultaneous
  /// monitor/chiller/DH updates into a single write so the 7-day log keeps
  /// ~2 points/min instead of one per stream event.
  static const _minLogGap = Duration(seconds: 30);

  @override
  void initState() {
    super.initState();
    _firebase = RadiantFirebase();
    _startStreamListeners();
    _load();
  }

  void _startStreamListeners() {
    _subs.add(_firebase.monitorStream().listen((m) {
      _lastMonitor = m;
      _logSnapshot();
      final wasCount = _unreadAlertCount;
      _alertService.processMonitorTelemetry(
        coldestPipeC: m.coldestPipeC,
        waterFloorC: m.waterFloorC,
      );
      _checkAlertCount(wasCount);
    }));

    _subs.add(_firebase.chillerStream().listen((ch) {
      _lastChiller = ch;
      _logSnapshot();
    }));

    _subs.add(_firebase.dhStream().listen((dh) {
      _lastDh = dh;
      _logSnapshot();
    }));

    _subs.add(_firebase.heartbeatStream().listen((hb) {
      final wasCount = _unreadAlertCount;
      _alertService.processHeartbeat(
        online: hb.online,
        deviceId: hb.deviceId ?? 'unknown',
      );
      _checkAlertCount(wasCount);
    }));
  }

  /// Write one telemetry point built from the latest monitor / chiller / DH
  /// readings, so every point carries the room (DHT22) temperature and
  /// humidity the trends charts plot. Updates arriving within
  /// [_minLogGap] of the last write are coalesced into a single trailing
  /// write instead of racing each other on the log file.
  void _logSnapshot() {
    final last = _lastLogAt;
    final elapsed = last == null ? null : DateTime.now().difference(last);
    if (elapsed != null && elapsed < _minLogGap) {
      _logDebounce ??= Timer(_minLogGap - elapsed, () {
        _logDebounce = null;
        _writeSnapshot();
      });
      return;
    }
    _writeSnapshot();
  }

  void _writeSnapshot() {
    final m = _lastMonitor;
    final ch = _lastChiller;
    final dh = _lastDh;
    if (m == null && ch == null && dh == null) return;
    final ts = DateTime.now();
    _lastLogAt = ts;
    _telemetryLogger.log(TelemetryPoint(
      timestamp: ts,
      supplyC: m?.supplyC,
      returnC: m?.returnC,
      coldestPipeC: m?.coldestPipeC,
      tankTempC: ch?.waterTempC,
      indoorTempC: dh?.tempC,
      indoorHumidityPct: dh?.humidityPct,
      outdoorTempC: m?.outdoorTempC,
      outdoorDewPointC: m?.outdoorDewPointC,
    ));
  }

  void _checkAlertCount(int previousCount) async {
    final alerts = await _alertService.load();
    if (!mounted) return;
    if (alerts.length > previousCount && _tabIndex != 2) {
      setState(() => _unreadAlertCount = alerts.length - previousCount);
    }
  }

  @override
  void dispose() {
    _logDebounce?.cancel();
    _watchdog?.dispose();
    for (final sub in _subs) {
      sub.cancel();
    }
    _subs.clear();
    super.dispose();
  }

  /// The watchdog only makes sense once a system is linked, and it must follow
  /// a re-link, so it is rebuilt whenever the linked id changes.
  void _restartWatchdog() {
    _watchdog?.dispose();
    final id = _linkedId;
    _watchdog = id == null
        ? null
        : (DeviceWatchdog(firebase: _firebase, systemId: id)..start());
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
    _restartWatchdog();
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

  /// The initial key for the dialog: the local copy when present, otherwise
  /// whatever is already live in Firebase (set earlier or from another
  /// device) - so the dialog never shows blank when the gateway has a key.
  Future<String?> _resolveInitialKey() async {
    final local = _weatherKey;
    if (local != null && local.isNotEmpty) return local;
    try {
      return await _firebase.fetchWeatherKey();
    } catch (_) {
      return null;
    }
  }

  Future<void> _manageKey() async {
    final initial = await _resolveInitialKey();
    if (!mounted) return;
    final controller = TextEditingController(text: initial ?? '');
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

  Future<void> _linkAndSave(String id) async {
    await _deviceLink.save(id);
    if (!mounted) return;
    setState(() => _linkedId = id);
    _restartWatchdog();
    if (_weatherKey != null && _weatherKey!.isNotEmpty) {
      await _publishKey(_weatherKey!);
    }
  }

  void _onLinkedFromScreen(String id) {
    if (!mounted) return;
    setState(() => _linkedId = id);
    _restartWatchdog();
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
      alertBadgeCount: _unreadAlertCount,
      onTabChanged: (i) {
        setState(() {
          _tabIndex = i;
          if (i == 2) _unreadAlertCount = 0;
        });
      },
      children: [
        DashboardScreen(
          firebase: _firebase,
          linkedId: _linkedId,
          onRefresh: () {},
        ),
        TrendsScreen(firebase: _firebase, logger: _telemetryLogger),
        AlertsScreen(alertService: _alertService),
        SettingsScreen(
          firebase: _firebase,
          linkedId: _linkedId,
          weatherKey: _weatherKey,
          onLinkSystem: _linkSystem,
          onManageKey: _manageKey,
          onSignOut: _signOut,
          onNotifications: () {
            Navigator.push(
              context,
              MaterialPageRoute(
                builder: (_) => NotificationSettingsScreen(
                  service: _notificationService,
                ),
              ),
            );
          },
          onAbout: () {
            Navigator.push(
              context,
              MaterialPageRoute(
                builder: (_) => AboutScreen(
                  firebase: _firebase,
                  linkedId: _linkedId,
                ),
              ),
            );
          },
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
