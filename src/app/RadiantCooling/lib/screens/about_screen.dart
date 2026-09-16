import 'package:flutter/material.dart';
import 'package:url_launcher/url_launcher.dart';

import '../models/telemetry.dart';
import '../services/radiant_firebase.dart';
import '../widgets/app_logo.dart';
import '../widgets/section_card.dart';

class AboutScreen extends StatelessWidget {
  const AboutScreen({
    super.key,
    required this.firebase,
    required this.linkedId,
  });

  final RadiantFirebase firebase;
  final String? linkedId;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(title: const Text('About')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          const Center(child: AppLogo(size: 80)),
          const SizedBox(height: 8),
          Center(
            child: Text(
              'Radiant Cooling',
              style: theme.textTheme.headlineSmall?.copyWith(
                fontWeight: FontWeight.bold,
              ),
            ),
          ),
          const SizedBox(height: 4),
          Center(
            child: Text(
              'v1.0.0+1',
              style: theme.textTheme.bodyMedium?.copyWith(
                color: theme.colorScheme.outline,
              ),
            ),
          ),
          const SizedBox(height: 24),

          if (linkedId != null)
            StreamBuilder<Heartbeat>(
              stream: firebase.heartbeatStream(),
              builder: (context, snap) {
                final hb = snap.data;
                return SectionCard(
                  title: 'Linked system',
                  icon: Icons.link,
                  child: Column(
                    children: [
                      _InfoRow(label: 'System ID', value: linkedId!),
                      _InfoRow(
                        label: 'Status',
                        value: (hb?.online ?? false) ? 'Online' : 'Offline',
                        valueColor: (hb?.online ?? false)
                            ? Colors.green.shade600
                            : theme.colorScheme.error,
                      ),
                      _InfoRow(
                        label: 'Device ID',
                        value: hb?.deviceId ?? '—',
                      ),
                    ],
                  ),
                );
              },
            ),
          const SizedBox(height: 12),

          SectionCard(
            title: 'Hardware',
            icon: Icons.memory,
            child: Column(
              children: [
                _DeviceTile(
                  name: 'RadiantCoolingMonitor',
                  role: 'Gateway',
                  sensors: '6× DS18B20 (pipe temps)',
                  actuators: '—',
                ),
                const Divider(),
                _DeviceTile(
                  name: 'WaterChillerController',
                  role: 'Peer',
                  sensors: '3× DS18B20 (water temps)',
                  actuators: '2× SSR → water pumps',
                ),
                const Divider(),
                _DeviceTile(
                  name: 'DehumidifierController',
                  role: 'Peer',
                  sensors: '1× DHT22 (indoor temp + humidity)',
                  actuators: '1× SSR → dehumidifier',
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),

          SectionCard(
            title: 'Technology',
            icon: Icons.code,
            child: Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                _Badge(label: 'ESP32', color: Colors.blue),
                _Badge(label: 'ESP-NOW', color: Colors.cyan),
                _Badge(label: 'Flutter', color: Colors.lightBlue),
                _Badge(label: 'Firebase', color: Colors.amber),
                _Badge(label: 'Arduino', color: Colors.teal),
              ],
            ),
          ),
          const SizedBox(height: 12),

          SectionCard(
            title: 'Credits',
            icon: Icons.person_outline,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'Sajed Lopez Mendoza',
                  style: theme.textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  'Building intelligent solutions',
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: theme.colorScheme.onSurfaceVariant,
                  ),
                ),
                const SizedBox(height: 12),
                Row(
                  children: [
                    OutlinedButton.icon(
                      onPressed: () => _launchUrl(
                        'https://github.com/qppd/radiant-cooling',
                      ),
                      icon: const Icon(Icons.code, size: 18),
                      label: const Text('GitHub'),
                    ),
                    const SizedBox(width: 8),
                    OutlinedButton.icon(
                      onPressed: () => _launchUrl(
                        'https://www.linkedin.com/in/sajed-mendoza',
                      ),
                      icon: const Icon(Icons.link, size: 18),
                      label: const Text('LinkedIn'),
                    ),
                  ],
                ),
              ],
            ),
          ),
          const SizedBox(height: 12),

          Center(
            child: Text(
              'MIT License',
              style: theme.textTheme.bodySmall?.copyWith(
                color: theme.colorScheme.outline,
              ),
            ),
          ),
          const SizedBox(height: 24),
        ],
      ),
    );
  }

  Future<void> _launchUrl(String url) async {
    final uri = Uri.parse(url);
    if (await canLaunchUrl(uri)) {
      await launchUrl(uri, mode: LaunchMode.externalApplication);
    }
  }
}

class _InfoRow extends StatelessWidget {
  const _InfoRow({
    required this.label,
    required this.value,
    this.valueColor,
  });

  final String label;
  final String value;
  final Color? valueColor;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          Expanded(
            child: Text(label, style: Theme.of(context).textTheme.bodyMedium),
          ),
          Text(
            value,
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
              fontWeight: FontWeight.w600,
              color: valueColor,
            ),
          ),
        ],
      ),
    );
  }
}

class _DeviceTile extends StatelessWidget {
  const _DeviceTile({
    required this.name,
    required this.role,
    required this.sensors,
    required this.actuators,
  });

  final String name;
  final String role;
  final String sensors;
  final String actuators;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Expanded(
              child: Text(
                name,
                style: theme.textTheme.titleSmall?.copyWith(
                  fontWeight: FontWeight.w600,
                ),
              ),
            ),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
              decoration: BoxDecoration(
                color: theme.colorScheme.primaryContainer,
                borderRadius: BorderRadius.circular(12),
              ),
              child: Text(
                role,
                style: theme.textTheme.labelSmall?.copyWith(
                  color: theme.colorScheme.onPrimaryContainer,
                ),
              ),
            ),
          ],
        ),
        const SizedBox(height: 4),
        Text('Sensors: $sensors', style: theme.textTheme.bodySmall),
        Text('Actuators: $actuators', style: theme.textTheme.bodySmall),
      ],
    );
  }
}

class _Badge extends StatelessWidget {
  const _Badge({required this.label, required this.color});
  final String label;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: color.withValues(alpha: 0.3)),
      ),
      child: Text(
        label,
        style: TextStyle(
          color: color,
          fontWeight: FontWeight.w600,
          fontSize: 12,
        ),
      ),
    );
  }
}
