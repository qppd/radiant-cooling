import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';

import 'config/app_config.dart';
import 'services/device_link.dart';
import 'services/radiant_firebase.dart';
import 'services/weather_key_store.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp();
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
      home: const HomeScreen(),
    );
  }
}

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final _deviceLink = DeviceLink();
  final _keyStore = WeatherKeyStore();
  late final RadiantFirebase _firebase;

  String? _linkedId;
  String? _weatherKey;
  List<String> _knownSystems = const [];

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
    final discovered = await _firebase.discoverSystems();
    if (!mounted) return;
    setState(() => _knownSystems = discovered);

    final controller =
        TextEditingController(text: _linkedId ?? AppConfig.defaultSystemId);
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
            if (_knownSystems.isNotEmpty) ...[
              const SizedBox(height: 12),
              Wrap(
                spacing: 6,
                children: [
                  for (final id in _knownSystems)
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
    await _deviceLink.save(id);
    if (!mounted) return;
    setState(() => _linkedId = id);
    // Re-deliver the key now that a system is linked.
    if (_weatherKey != null && _weatherKey!.isNotEmpty) {
      await _publishKey(_weatherKey!);
    }
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

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Radiant Cooling')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: ListTile(
              leading: Icon(_linkedId == null ? Icons.link_off : Icons.link),
              title: Text(_linkedId == null ? 'Not linked' : _linkedId!),
              subtitle: const Text(
                'System ID connecting this app to the ESP32 gateway '
                'via Firebase',
              ),
              trailing: TextButton(
                onPressed: _linkSystem,
                child: Text(_linkedId == null ? 'Link' : 'Change'),
              ),
            ),
          ),
          const SizedBox(height: 12),
          Card(
            child: ListTile(
              leading: Icon(
                _weatherKey == null ? Icons.key_off : Icons.key,
              ),
              title: Text(
                _weatherKey == null ? 'No WeatherAPI key' : 'WeatherAPI key set',
              ),
              subtitle: const Text(
                'Managed from this app and delivered to the gateway '
                '(radiant/config/weather_key). The ESP32 fetches the '
                'weather itself.',
              ),
              trailing: TextButton(
                onPressed: _manageKey,
                child: Text(_weatherKey == null ? 'Set key' : 'Change'),
              ),
            ),
          ),
        ],
      ),
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
                icon: Icon(
                  _obscure ? Icons.visibility : Icons.visibility_off,
                ),
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
          onPressed: () => Navigator.pop(context, widget.controller.text.trim()),
          child: const Text('Save'),
        ),
      ],
    );
  }
}
