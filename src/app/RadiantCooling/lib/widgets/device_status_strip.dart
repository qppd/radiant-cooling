import 'package:flutter/material.dart';

import '../models/telemetry.dart';
import '../services/radiant_firebase.dart';

class DeviceStatusStrip extends StatelessWidget {
  const DeviceStatusStrip({
    super.key,
    required this.firebase,
  });

  final RadiantFirebase firebase;

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<Heartbeat>(
      stream: firebase.heartbeatStream(),
      builder: (context, hbSnap) {
        final hb = hbSnap.data;
        final gwOnline = hb?.online ?? false;
        return StreamBuilder<ChillerTelemetry>(
          stream: firebase.chillerStream(),
          builder: (context, chSnap) {
            final chOnline = chSnap.data?.ts != null;
            return StreamBuilder<DhTelemetry>(
              stream: firebase.dhStream(),
              builder: (context, dhSnap) {
                final dhOnline = dhSnap.data?.ts != null;
                return Card(
                  child: Padding(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 12,
                      vertical: 10,
                    ),
                    child: Row(
                      children: [
                        _DeviceChip(
                          label: 'Monitor',
                          icon: Icons.router,
                          online: gwOnline,
                          firmware: hb?.deviceId != null ? 'v0.1.0' : null,
                        ),
                        const SizedBox(width: 8),
                        _DeviceChip(
                          label: 'Chiller',
                          icon: Icons.thermostat,
                          online: chOnline,
                        ),
                        const SizedBox(width: 8),
                        _DeviceChip(
                          label: 'Dehumidifier',
                          icon: Icons.water_drop_outlined,
                          online: dhOnline,
                        ),
                      ],
                    ),
                  ),
                );
              },
            );
          },
        );
      },
    );
  }
}

class _DeviceChip extends StatelessWidget {
  const _DeviceChip({
    required this.label,
    required this.icon,
    required this.online,
    this.firmware,
  });

  final String label;
  final IconData icon;
  final bool online;
  final String? firmware;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final color = online ? Colors.green.shade600 : theme.colorScheme.outline;
    return Expanded(
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        decoration: BoxDecoration(
          color: online
              ? Colors.green.withValues(alpha: 0.08)
              : theme.colorScheme.surfaceContainerHighest,
          borderRadius: BorderRadius.circular(8),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 16, color: color),
            const SizedBox(width: 6),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                mainAxisSize: MainAxisSize.min,
                children: [
                  Text(
                    label,
                    style: theme.textTheme.labelMedium?.copyWith(
                      fontWeight: FontWeight.w600,
                    ),
                    overflow: TextOverflow.ellipsis,
                  ),
                  Text(
                    online ? 'Online' : 'Offline',
                    style: theme.textTheme.labelSmall?.copyWith(color: color),
                  ),
                ],
              ),
            ),
            Container(
              width: 8,
              height: 8,
              decoration: BoxDecoration(
                color: color,
                shape: BoxShape.circle,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
