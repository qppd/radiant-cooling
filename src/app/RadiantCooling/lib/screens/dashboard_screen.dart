import 'dart:math';

import 'package:flutter/material.dart';

import '../models/telemetry.dart';
import '../services/dew_point.dart';
import '../services/radiant_firebase.dart';
import '../widgets/section_card.dart';

/// Live dashboard: system status, weather, cooling loop, control, and
/// condensation-safety values streamed from Firebase.
class DashboardScreen extends StatelessWidget {
  const DashboardScreen({
    super.key,
    required this.firebase,
    required this.linkedId,
  });

  final RadiantFirebase firebase;
  final String? linkedId;

  String _fmt(double? v, {int digits = 1}) =>
      v == null || v <= -90 ? '—' : '${v.toStringAsFixed(digits)} °C';

  String _time(int? ts) {
    if (ts == null) return '—';
    final t = DateTime.fromMillisecondsSinceEpoch(ts * 1000).toLocal();
    final h = t.hour.toString().padLeft(2, '0');
    final m = t.minute.toString().padLeft(2, '0');
    return '$h:$m';
  }

  @override
  Widget build(BuildContext context) {
    if (linkedId == null) {
      return const _NotLinkedView();
    }
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _StatusCard(
          stream: firebase.heartbeatStream(),
          linkedId: linkedId!,
          time: _time,
        ),
        const SizedBox(height: 12),
        SectionCard(
          title: 'Outdoor weather',
          icon: Icons.wb_sunny_outlined,
          child: StreamBuilder<MonitorTelemetry>(
            stream: firebase.monitorStream(),
            builder: (context, snap) {
              final t = snap.data;
              return _MetricRow(
                children: [
                  _Metric(label: 'Temp', value: _fmt(t?.outdoorTempC)),
                  _Metric(label: 'Dew point', value: _fmt(t?.outdoorDewPointC)),
                  _Metric(
                    label: 'Humidity',
                    value: (t?.outdoorHumidityPct ?? 0) <= -90
                        ? '—'
                        : '${(t?.outdoorHumidityPct ?? 0).toStringAsFixed(0)} %',
                  ),
                ],
              );
            },
          ),
        ),
        const SizedBox(height: 12),
        SectionCard(
          title: 'Cooling loop',
          icon: Icons.water_drop_outlined,
          child: StreamBuilder<MonitorTelemetry>(
            stream: firebase.monitorStream(),
            builder: (context, snap) {
              final t = snap.data;
              return StreamBuilder<ChillerTelemetry>(
                stream: firebase.chillerStream(),
                builder: (context, csnap) {
                  final ch = csnap.data;
                  return _MetricRow(
                    children: [
                      _Metric(label: 'Supply', value: _fmt(t?.supplyC)),
                      _Metric(label: 'Return', value: _fmt(t?.returnC)),
                      _Metric(label: 'ΔT', value: _fmt(t?.deltaTC)),
                      _Metric(
                        label: 'Coldest pipe',
                        value: _fmt(t?.coldestPipeC),
                      ),
                      _Metric(label: 'Tank', value: _fmt(ch?.waterTempC)),
                    ],
                  );
                },
              );
            },
          ),
        ),
        const SizedBox(height: 12),
        SectionCard(
          title: 'Pipe sensors (DS18B20)',
          icon: Icons.sensors,
          child: StreamBuilder<MonitorTelemetry>(
            stream: firebase.monitorStream(),
            builder: (context, snap) {
              final t = snap.data;
              final temps = t?.tempsC ?? const <double?>[];
              if (temps.isEmpty) {
                return Text(
                  'No pipe readings yet',
                  style: Theme.of(context).textTheme.bodySmall,
                );
              }
              const labels = [
                'Supply',
                'Return',
                'Pipe 1',
                'Pipe 2',
                'Pipe 3',
                'Pipe 4',
              ];
              return _MetricRow(
                children: [
                  for (var i = 0; i < temps.length && i < labels.length; i++)
                    _Metric(label: labels[i], value: _fmt(temps[i])),
                ],
              );
            },
          ),
        ),
        const SizedBox(height: 12),
        SectionCard(
          title: 'Control',
          icon: Icons.power_settings_new,
          child: StreamBuilder<ChillerState>(
            stream: firebase.chillerStateStream(),
            builder: (context, chSnap) {
              final ch = chSnap.data;
              return StreamBuilder<DhTelemetry>(
                stream: firebase.dhStream(),
                builder: (context, dhSnap) {
                  final dh = dhSnap.data;
                  return StreamBuilder<DhState>(
                    stream: firebase.dhStateStream(),
                    builder: (context, stSnap) {
                      final dehumOn = stSnap.data?.on ?? false;
                      return Column(
                        children: [
                          _StatusTile(
                            label: 'Chiller pump 1',
                            on: ch?.pump1On ?? false,
                          ),
                          const SizedBox(height: 8),
                          _StatusTile(
                            label: 'Chiller pump 2',
                            on: ch?.pump2On ?? false,
                          ),
                          const SizedBox(height: 8),
                          _StatusTile(label: 'Dehumidifier', on: dehumOn),
                          const SizedBox(height: 12),
                          _MetricRow(
                            children: [
                              _Metric(
                                label: 'Indoor temp',
                                value: _fmt(dh?.tempC),
                              ),
                              _Metric(
                                label: 'Indoor humidity',
                                value: (dh?.humidityPct ?? 0) <= 0
                                    ? '—'
                                    : '${(dh?.humidityPct ?? 0).toStringAsFixed(0)} %',
                              ),
                            ],
                          ),
                        ],
                      );
                    },
                  );
                },
              );
            },
          ),
        ),
        const SizedBox(height: 12),
        SectionCard(
          title: 'Dew point (computed live)',
          icon: Icons.shield_outlined,
          child: StreamBuilder<MonitorTelemetry>(
            stream: firebase.monitorStream(),
            builder: (context, snap) {
              final t = snap.data;
              return StreamBuilder<DhTelemetry>(
                stream: firebase.dhStream(),
                builder: (context, dhSnap) {
                  final dh = dhSnap.data;
                  return StreamBuilder<ControlParams>(
                    stream: firebase.controlParamsStream(),
                    builder: (context, cSnap) {
                      // Recompute the gateway's decision client-side from
                      // the live streamed sensors (Magnus formula): the
                      // reference dew point is the higher of outdoor and
                      // indoor, and the water floor is that + the margin.
                      final outdoor = dewPointC(
                        t?.outdoorTempC,
                        t?.outdoorHumidityPct,
                      );
                      final indoor = dewPointC(dh?.tempC, dh?.humidityPct);
                      final ref = _higher(outdoor, indoor);
                      final params = cSnap.data ?? const ControlParams();
                      final floor = ref == null
                          ? null
                          : ref + params.dewpointMarginC;
                      final margin = params.dewpointMarginC;
                      return Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          _MetricRow(
                            children: [
                              _Metric(
                                label: 'Outdoor DP',
                                value: _fmt(outdoor),
                              ),
                              _Metric(label: 'Indoor DP', value: _fmt(indoor)),
                              _Metric(
                                label: 'Ref DP',
                                value: _fmt(ref),
                                highlighted: true,
                              ),
                              _Metric(
                                label: 'Water floor',
                                value: _fmt(floor),
                                highlighted: true,
                              ),
                            ],
                          ),
                          const SizedBox(height: 8),
                          Text(
                            'Ref DP = max(outdoor, indoor) · floor = '
                            'ref + margin ($margin °C) — computed from '
                            'the live Firebase stream.',
                            style: Theme.of(context).textTheme.bodySmall
                                ?.copyWith(
                                  color: Theme.of(context).colorScheme.outline,
                                ),
                          ),
                        ],
                      );
                    },
                  );
                },
              );
            },
          ),
        ),
        const SizedBox(height: 12),
        SectionCard(
          title: 'Condensation safety (gateway)',
          icon: Icons.verified_outlined,
          child: StreamBuilder<MonitorTelemetry>(
            stream: firebase.monitorStream(),
            builder: (context, snap) {
              final t = snap.data;
              return _MetricRow(
                children: [
                  _Metric(label: 'Ref dew point', value: _fmt(t?.dewPointC)),
                  _Metric(
                    label: 'Water floor',
                    value: _fmt(t?.waterFloorC),
                    highlighted: true,
                  ),
                ],
              );
            },
          ),
        ),
        const SizedBox(height: 12),
        StreamBuilder<Heartbeat>(
          stream: firebase.heartbeatStream(),
          builder: (context, snap) {
            return Center(
              child: Text(
                'Last update: ${_time(snap.data?.ts)} · '
                'device ${snap.data?.deviceId ?? linkedId}',
                style: Theme.of(context).textTheme.bodySmall,
              ),
            );
          },
        ),
      ],
    );
  }
}

class _NotLinkedView extends StatelessWidget {
  const _NotLinkedView();

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(
            Icons.link_off,
            size: 56,
            color: Theme.of(context).colorScheme.outline,
          ),
          const SizedBox(height: 12),
          const Text('No system linked yet'),
          const SizedBox(height: 4),
          Text(
            'Go to Settings and enter the SYSTEM_ID of your gateway.',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      ),
    );
  }
}

double? _higher(double? a, double? b) {
  if (a == null && b == null) return null;
  if (a == null) return b;
  if (b == null) return a;
  return max(a, b);
}

class _StatusCard extends StatelessWidget {
  const _StatusCard({
    required this.stream,
    required this.linkedId,
    required this.time,
  });

  final Stream<Heartbeat> stream;
  final String linkedId;
  final String Function(int?) time;

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<Heartbeat>(
      stream: stream,
      builder: (context, snap) {
        final hb = snap.data;
        final online = hb?.online ?? false;
        return Card(
          color: online
              ? Theme.of(context).colorScheme.primaryContainer
              : Theme.of(context).colorScheme.surfaceContainerHighest,
          child: ListTile(
            leading: Icon(
              online ? Icons.check_circle : Icons.error_outline,
              color: online
                  ? Colors.green.shade700
                  : Theme.of(context).colorScheme.error,
            ),
            title: Text(online ? 'Gateway online' : 'Gateway offline'),
            subtitle: Text('System $linkedId · heartbeat ${time(hb?.ts)}'),
            trailing: const Icon(Icons.router),
          ),
        );
      },
    );
  }
}

class _MetricRow extends StatelessWidget {
  const _MetricRow({required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    return Wrap(spacing: 24, runSpacing: 12, children: children);
  }
}

class _Metric extends StatelessWidget {
  const _Metric({
    required this.label,
    required this.value,
    this.highlighted = false,
  });

  final String label;
  final String value;
  final bool highlighted;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: theme.textTheme.bodySmall),
        Text(
          value,
          style: theme.textTheme.titleLarge?.copyWith(
            color: highlighted ? theme.colorScheme.primary : null,
            fontWeight: highlighted ? FontWeight.bold : null,
          ),
        ),
      ],
    );
  }
}

class _StatusTile extends StatelessWidget {
  const _StatusTile({required this.label, required this.on});

  final String label;
  final bool on;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Icon(
          on ? Icons.power : Icons.power_off,
          size: 18,
          color: on
              ? Colors.green.shade700
              : Theme.of(context).colorScheme.outline,
        ),
        const SizedBox(width: 8),
        Text(label),
        const Spacer(),
        Text(
          on ? 'ON' : 'OFF',
          style: Theme.of(context).textTheme.labelLarge?.copyWith(
            color: on
                ? Colors.green.shade700
                : Theme.of(context).colorScheme.outline,
            fontWeight: FontWeight.bold,
          ),
        ),
      ],
    );
  }
}
