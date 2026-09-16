import 'package:flutter/material.dart';

import '../config/app_config.dart';
import '../services/device_link.dart';
import '../services/radiant_firebase.dart';
import '../widgets/app_logo.dart';

class LinkDeviceScreen extends StatefulWidget {
  const LinkDeviceScreen({
    super.key,
    required this.firebase,
    required this.deviceLink,
    required this.onLinked,
  });

  final RadiantFirebase firebase;
  final DeviceLink deviceLink;

  final ValueChanged<String> onLinked;

  @override
  State<LinkDeviceScreen> createState() => _LinkDeviceScreenState();
}

class _LinkDeviceScreenState extends State<LinkDeviceScreen> {
  final _controller = TextEditingController(text: AppConfig.defaultSystemId);

  List<String> _knownSystems = const [];
  bool _loading = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    _discover();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  Future<void> _discover() async {
    try {
      final systems = await widget.firebase.discoverSystems();
      if (!mounted) return;
      setState(() => _knownSystems = systems);
    } catch (_) {
    }
  }

  Future<void> _link() async {
    final id = _controller.text.trim();
    if (id.isEmpty) {
      setState(() => _error = 'Enter the system ID from the gateway.');
      return;
    }
    setState(() {
      _loading = true;
      _error = null;
    });

    bool known = false;
    try {
      known = await widget.firebase.isKnownSystem(id);
    } catch (_) {
      known = false;
    }

    await widget.deviceLink.save(id);
    if (!mounted) return;
    setState(() => _loading = false);

    if (!known) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text(
            'System not found in the registry — is the gateway online? '
            'You can still continue and link it again later.',
          ),
        ),
      );
    }
    widget.onLinked(id);
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Scaffold(
      appBar: AppBar(title: const Text('Link your device')),
      body: Center(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(24),
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 480),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Center(child: AppLogo(size: 88)),
                const SizedBox(height: 12),
                Text(
                  'Link your Radiant Cooling system',
                  textAlign: TextAlign.center,
                  style: theme.textTheme.headlineSmall?.copyWith(
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 8),
                Text(
                  'Enter the SYSTEM_ID configured on your gateway '
                  '(SYSTEM_ID in Config.h, printed on the device). '
                  'This connects this app to your installation via Firebase.',
                  textAlign: TextAlign.center,
                  style: theme.textTheme.bodyMedium?.copyWith(
                    color: theme.colorScheme.onSurfaceVariant,
                  ),
                ),
                const SizedBox(height: 20),
                TextField(
                  controller: _controller,
                  enabled: !_loading,
                  decoration: const InputDecoration(
                    labelText: 'System ID',
                    hintText: 'e.g. RADIANT-001',
                    prefixIcon: Icon(Icons.link),
                    border: OutlineInputBorder(),
                  ),
                  textCapitalization: TextCapitalization.characters,
                ),
                if (_knownSystems.isNotEmpty) ...[
                  const SizedBox(height: 12),
                  Text(
                    'Found on this Firebase project:',
                    style: theme.textTheme.bodySmall?.copyWith(
                      color: theme.colorScheme.onSurfaceVariant,
                    ),
                  ),
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 6,
                    children: [
                      for (final id in _knownSystems)
                        ActionChip(
                          label: Text(id),
                          onPressed: _loading
                              ? null
                              : () => _controller.text = id,
                        ),
                    ],
                  ),
                ],
                if (_error != null) ...[
                  const SizedBox(height: 12),
                  Container(
                    padding: const EdgeInsets.all(12),
                    decoration: BoxDecoration(
                      color: theme.colorScheme.errorContainer,
                      borderRadius: BorderRadius.circular(10),
                    ),
                    child: Row(
                      children: [
                        Icon(
                          Icons.error_outline,
                          color: theme.colorScheme.onErrorContainer,
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          child: Text(
                            _error!,
                            style: TextStyle(
                              color: theme.colorScheme.onErrorContainer,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
                const SizedBox(height: 20),
                FilledButton.icon(
                  onPressed: _loading ? null : _link,
                  style: FilledButton.styleFrom(
                    padding: const EdgeInsets.symmetric(vertical: 16),
                  ),
                  icon: _loading
                      ? const SizedBox(
                          height: 20,
                          width: 20,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.check_circle_outline),
                  label: Text(_loading ? 'Linking…' : 'Link this device'),
                ),
                const SizedBox(height: 12),
                Text(
                  'The gateway publishes its ID to the registry when it '
                  'is online. If your system does not appear yet, check '
                  'that the gateway is powered on.',
                  textAlign: TextAlign.center,
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: theme.colorScheme.outline,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
