import 'dart:async';

import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';

import 'config/app_config.dart';
import 'services/device_link.dart';
import 'services/radiant_firebase.dart';
import 'services/weather_service.dart';

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
  late final RadiantFirebase _firebase;
  final _weather = WeatherService();

  String? _linkedId;
  WeatherConditions _wx = const WeatherConditions(ok: false);
  Timer? _weatherTimer;
  List<String> _knownSystems = const [];

  @override
  void initState() {
    super.initState();
    _firebase = RadiantFirebase();
    _load();
  }

  @override
  void dispose() {
    _weatherTimer?.cancel();
    super.dispose();
  }

  Future<void> _load() async {
    final id = await _deviceLink.load();
    if (!mounted) return;
    setState(() => _linkedId = id);
    if (id != null) _startWeatherPolling();
  }

  void _startWeatherPolling() {
    _weatherTimer?.cancel();
    _refreshWeather();
    _weatherTimer = Timer.periodic(
      Duration(minutes: AppConfig.weatherPollMinutes),
      (_) => _refreshWeather(),
    );
  }

  Future<void> _refreshWeather() async {
    final wx = await _weather.fetchCurrent();
    if (!mounted) return;
    setState(() => _wx = wx);
    // Publish to Firebase only once a system is linked; the gateway streams
    // this node and uses it for the pump decision.
    if (wx.ok && _linkedId != null) {
      try {
        await _firebase.publishWeather(wx);
      } catch (_) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text(
                'Could not publish weather — check the Firebase setup '
                '(google-services.json).',
              ),
            ),
          );
        }
      }
    }
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
    _startWeatherPolling();
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
    final theme = Theme.of(context);
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
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      const Icon(Icons.wb_sunny_outlined),
                      const SizedBox(width: 8),
                      Text('Outdoor weather', style: theme.textTheme.titleMedium),
                      const Spacer(),
                      IconButton(
                        tooltip: 'Refresh',
                        onPressed: _refreshWeather,
                        icon: const Icon(Icons.refresh),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  if (_wx.ok)
                    Wrap(
                      spacing: 24,
                      runSpacing: 8,
                      children: [
                        _Metric(
                          label: 'Temp',
                          value: '${_wx.tempC.toStringAsFixed(1)} °C',
                        ),
                        _Metric(
                          label: 'Dew point',
                          value: '${_wx.dewPointC.toStringAsFixed(1)} °C',
                        ),
                        _Metric(
                          label: 'Humidity',
                          value: '${_wx.humidityPct.toStringAsFixed(0)} %',
                        ),
                      ],
                    )
                  else
                    const Text(
                      'Weather not available — check the API key in '
                      'lib/config/app_config.dart.',
                    ),
                  const SizedBox(height: 8),
                  Text(
                    _linkedId == null
                        ? 'Link a system to start publishing weather to the '
                            'gateway (radiant/config/weather).'
                        : 'Published to radiant/config/weather for '
                            '$_linkedId.',
                    style: theme.textTheme.bodySmall,
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _Metric extends StatelessWidget {
  const _Metric({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: Theme.of(context).textTheme.bodySmall),
        Text(value, style: Theme.of(context).textTheme.titleLarge),
      ],
    );
  }
}
