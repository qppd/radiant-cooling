import 'dart:math';

import 'package:flutter/material.dart';

import '../models/telemetry.dart';
import '../services/dew_point.dart';
import '../services/radiant_firebase.dart';
import '../widgets/device_status_strip.dart';
import '../widgets/section_card.dart';

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({
    super.key,
    required this.firebase,
    required this.linkedId,
    this.onRefresh,
  });

  final RadiantFirebase firebase;
  final String? linkedId;
  final VoidCallback? onRefresh;

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  @override
  void initState() {
    super.initState();
    // Keep the Firebase service aware of the current linked system so
    // control writes hit the right path.
    if (widget.linkedId != null) {
      widget.firebase.setLinkedId(widget.linkedId!);
    }
  }

  @override
  void didUpdateWidget(DashboardScreen old) {
    super.didUpdateWidget(old);
    if (widget.linkedId != old.linkedId && widget.linkedId != null) {
      widget.firebase.setLinkedId(widget.linkedId!);
    }
  }

  String _fmt(double? v, {int digits = 1}) =>
      v == null || v <= -90 ? '—' : '${v.toStringAsFixed(digits)} °C';

  String _time(int? ts) {
    if (ts == null) return '—';
    final t = DateTime.fromMillisecondsSinceEpoch(ts * 1000).toLocal();
    final h = t.hour.toString().padLeft(2, '0');
    final m = t.minute.toString().padLeft(2, '0');
    return '$h:$m';
  }

  Future<void> _togglePumps(BuildContext context, bool on) async {
    try {
      await widget.firebase.setPumps(on);
    } catch (_) {
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Could not set pump state')),
      );
    }
  }

  Future<void> _toggleMode(BuildContext context, String mode) async {
    try {
      await widget.firebase.setMode(mode);
    } catch (_) {
      if (!context.mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Could not change mode')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    if (widget.linkedId == null) {
      return const _NotLinkedView();
    }
    return RefreshIndicator(
      onRefresh: widget.onRefresh != null
          ? () async {
              widget.onRefresh!();
              await Future<void>.delayed(const Duration(milliseconds: 500));
            }
          : () async {},
      child: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          DeviceStatusStrip(firebase: widget.firebase),
          const SizedBox(height: 12),
          _StatusCard(
            stream: widget.firebase.heartbeatStream(),
            linkedId: widget.linkedId!,
            time: _time,
          ),
          const SizedBox(height: 12),
          SectionCard(
            title: 'Outdoor weather',
            icon: Icons.wb_sunny_outlined,
            child: StreamBuilder<MonitorTelemetry>(
              stream: widget.firebase.monitorStream(),
              builder: (context, snap) {
                final t = snap.data;
                return _MetricRow(
                  children: [
                    _Metric(label: 'Temp', value: _fmt(t?.outdoorTempC)),
                    _Metric(
                        label: 'Dew point', value: _fmt(t?.outdoorDewPointC)),
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
            title: 'Tank temps (WaterChiller)',
            icon: Icons.water_drop_outlined,
            child: StreamBuilder<ChillerTelemetry>(
              stream: widget.firebase.chillerStream(),
              builder: (context, csnap) {
                final ch = csnap.data;
                return _MetricRow(
                  children: [
                    _Metric(label: 'Outlet', value: _fmt(ch?.outgoingTempC)),
                    _Metric(label: 'Inlet', value: _fmt(ch?.ingoingTempC)),
                    _Metric(label: 'Coldest', value: _fmt(ch?.waterTempC)),
                  ],
                );
              },
            ),
          ),
          const SizedBox(height: 12),
          SectionCard(
            title: 'Pipe sensors (Ceiling DS18B20)',
            icon: Icons.sensors,
            child: StreamBuilder<MonitorTelemetry>(
              stream: widget.firebase.monitorStream(),
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
                  'Pipe 1',
                  'Pipe 2',
                  'Pipe 3',
                  'Pipe 4',
                ];
                return _MetricRow(
                  children: [
                    for (var i = 0;
                        i < temps.length && i < labels.length;
                        i++)
                      _Metric(label: labels[i], value: _fmt(temps[i])),
                  ],
                );
              },
            ),
          ),
          const SizedBox(height: 12),
          // ── Control section (pumps + auto/manual) ────────────────────
          SectionCard(
            title: 'Control',
            icon: Icons.power_settings_new,
            child: StreamBuilder<ChillerState>(
              stream: widget.firebase.chillerStateStream(),
              builder: (context, chSnap) {
                return StreamBuilder<DhTelemetry>(
                  stream: widget.firebase.dhStream(),
                  builder: (context, dhSnap) {
                    final dh = dhSnap.data;
                    return StreamBuilder<DhState>(
                      stream: widget.firebase.dhStateStream(),
                      builder: (context, stSnap) {
                        final dehumOn = stSnap.data?.on ?? false;
                        return StreamBuilder<MonitorTelemetry>(
                          stream: widget.firebase.monitorStream(),
                          builder: (context, monSnap) {
                            final te = monSnap.data;
                            final pumpsOn = te?.pumpsOn ?? false;
                            final mode = te?.mode ?? 'auto';
                            return Column(
                              crossAxisAlignment:
                                  CrossAxisAlignment.start,
                              children: [
                                Row(
                                  mainAxisAlignment:
                                      MainAxisAlignment.spaceBetween,
                                  children: [
                                    const Text('Pumps',
                                        style: TextStyle(
                                            fontWeight: FontWeight.w500)),
                                    const Text('Mode',
                                        style: TextStyle(
                                            fontWeight: FontWeight.w500)),
                                  ],
                                ),
                                const SizedBox(height: 8),
                                Row(
                                  children: [
                                    Expanded(
                                      child: OutlinedButton.icon(
                                        onPressed: !pumpsOn
                                            ? () => _togglePumps(context, true)
                                            : null,
                                        icon: const Icon(Icons.power,
                                            size: 18),
                                        label: const Text('Pumps ON'),
                                      ),
                                    ),
                                    const SizedBox(width: 8),
                                    Expanded(
                                      child: OutlinedButton.icon(
                                        onPressed: pumpsOn
                                            ? () => _togglePumps(context, false)
                                            : null,
                                        icon: const Icon(Icons.power_off,
                                            size: 18),
                                        label: const Text('Pumps OFF'),
                                      ),
                                    ),
                                  ],
                                ),
                                const SizedBox(height: 12),
                                Wrap(
                                  spacing: 8,
                                  runSpacing: 8,
                                  children: ['auto', 'manual']
                                      .map((m) => ChoiceChip(
                                            label: Text(m),
                                            selected: mode == m,
                                            onSelected: (sel) {
                                              if (sel) _toggleMode(context, m);
                                            },
                                          ))
                                      .toList(),
                                ),
                                const Divider(height: 24),
                                StreamBuilder<ChillerState>(
                                  stream:
                                      widget.firebase.chillerStateStream(),
                                  builder: (context, c2) {
                                    final s = c2.data;
                                    return Column(
                                      children: [
                                        _StatusTile(
                                            label: 'Compressor',
                                            on: s?.compressorOn ?? false),
                                        const SizedBox(height: 8),
                                        _StatusTile(
                                            label: 'Water pump',
                                            on: s?.pump1On ?? false),
                                        const SizedBox(height: 8),
                                        _StatusTile(
                                            label: 'Dehumidifier',
                                            on: dehumOn),
                                      ],
                                    );
                                  },
                                ),
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
                );
              },
            ),
          ),
          const SizedBox(height: 12),
          SectionCard(
            title: 'Dew point (computed live)',
            icon: Icons.shield_outlined,
            child: StreamBuilder<MonitorTelemetry>(
              stream: widget.firebase.monitorStream(),
              builder: (context, snap) {
                final t = snap.data;
                return StreamBuilder<DhTelemetry>(
                  stream: widget.firebase.dhStream(),
                  builder: (context, dhSnap) {
                    final dh = dhSnap.data;
                    return StreamBuilder<ControlParams>(
                      stream: widget.firebase.controlParamsStream(),
                      builder: (context, cSnap) {
                        final outdoor = dewPointC(
                          t?.outdoorTempC,
                          t?.outdoorHumidityPct,
                        );
                        final indoor = dewPointC(dh?.tempC, dh?.humidityPct);
                        final ref = _higher(outdoor, indoor);
                        final params =
                            cSnap.data ?? const ControlParams();
                        final floor = ref == null
                            ? null
                            : ref + params.dewpointMarginC;
                        final margin = params.dewpointMarginC;
                        return Column(
                          crossAxisAlignment:
                              CrossAxisAlignment.start,
                          children: [
                            _MetricRow(
                              children: [
                                _Metric(
                                  label: 'Outdoor DP',
                                  value: _fmt(outdoor),
                                ),
                                _Metric(
                                    label: 'Indoor DP', value: _fmt(indoor)),
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
                              style: Theme.of(context)
                                  .textTheme
                                  .bodySmall
                                  ?.copyWith(
                                    color: Theme.of(context)
                                        .colorScheme
                                        .outline,
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
              stream: widget.firebase.monitorStream(),
              builder: (context, snap) {
                final t = snap.data;
                return _MetricRow(
                  children: [
                    _Metric(
                        label: 'Ref dew point', value: _fmt(t?.dewPointC)),
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
            stream: widget.firebase.heartbeatStream(),
            builder: (context, snap) {
              return Center(
                child: Text(
                  'Last update: ${_time(snap.data?.ts)} · '
                  'device ${snap.data?.deviceId ?? widget.linkedId}',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              );
            },
          ),
        ],
      ),
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
            subtitle:
                Text('System $linkedId · heartbeat ${time(hb?.ts)}'),
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
