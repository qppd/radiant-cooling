import 'package:flutter/material.dart';

import '../models/alert.dart';
import '../services/alert_service.dart';

class AlertsScreen extends StatefulWidget {
  const AlertsScreen({super.key, required this.alertService});

  final AlertService alertService;

  @override
  State<AlertsScreen> createState() => _AlertsScreenState();
}

class _AlertsScreenState extends State<AlertsScreen> {
  AlertSeverity? _filter;
  List<Alert> _alerts = const [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final alerts = await widget.alertService.load();
    if (!mounted) return;
    setState(() {
      _alerts = alerts;
      _loading = false;
    });
  }

  Future<void> _clearAll() async {
    await widget.alertService.clear();
    if (!mounted) return;
    setState(() => _alerts = const []);
  }

  List<Alert> get _filtered {
    if (_filter == null) return _alerts;
    return [for (final a in _alerts) if (a.severity == _filter) a];
  }

  @override
  Widget build(BuildContext context) {
    if (_loading) {
      return const Center(child: CircularProgressIndicator());
    }

    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 0),
          child: Row(
            children: [
              _FilterChip(
                label: 'All',
                selected: _filter == null,
                onTap: () => setState(() => _filter = null),
              ),
              const SizedBox(width: 6),
              _FilterChip(
                label: 'Critical',
                selected: _filter == AlertSeverity.critical,
                color: Colors.red,
                onTap: () => setState(() => _filter = AlertSeverity.critical),
              ),
              const SizedBox(width: 6),
              _FilterChip(
                label: 'Warning',
                selected: _filter == AlertSeverity.warning,
                color: Colors.orange,
                onTap: () => setState(() => _filter = AlertSeverity.warning),
              ),
              const SizedBox(width: 6),
              _FilterChip(
                label: 'Info',
                selected: _filter == AlertSeverity.info,
                color: Colors.blue,
                onTap: () => setState(() => _filter = AlertSeverity.info),
              ),
            ],
          ),
        ),

        Expanded(
          child: _filtered.isEmpty
              ? Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(
                        Icons.check_circle_outline,
                        size: 56,
                        color: Theme.of(context).colorScheme.outline,
                      ),
                      const SizedBox(height: 12),
                      const Text('No alerts'),
                      const SizedBox(height: 4),
                      Text(
                        'Your system is running smoothly.',
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ],
                  ),
                )
              : ListView.builder(
                  padding: const EdgeInsets.all(16),
                  itemCount: _filtered.length,
                  itemBuilder: (context, i) => _AlertTile(
                    alert: _filtered[i],
                  ),
                ),
        ),
      ],
    );
  }
}

class _FilterChip extends StatelessWidget {
  const _FilterChip({
    required this.label,
    required this.selected,
    required this.onTap,
    this.color,
  });

  final String label;
  final bool selected;
  final VoidCallback onTap;
  final Color? color;

  @override
  Widget build(BuildContext context) {
    return FilterChip(
      label: Text(label),
      selected: selected,
      onSelected: (_) => onTap(),
      selectedColor: color?.withValues(alpha: 0.15),
      checkmarkColor: color,
      labelStyle: TextStyle(
        color: selected ? color ?? Theme.of(context).colorScheme.primary : null,
        fontWeight: selected ? FontWeight.w600 : null,
      ),
    );
  }
}

class _AlertTile extends StatelessWidget {
  const _AlertTile({required this.alert});
  final Alert alert;

  IconData _icon(AlertSeverity s) => switch (s) {
    AlertSeverity.critical => Icons.error,
    AlertSeverity.warning => Icons.warning_amber,
    AlertSeverity.info => Icons.info_outline,
  };

  Color _color(AlertSeverity s, BuildContext context) => switch (s) {
    AlertSeverity.critical => Colors.red.shade600,
    AlertSeverity.warning => Colors.orange.shade600,
    AlertSeverity.info => Theme.of(context).colorScheme.primary,
  };

  String _timeAgo(DateTime dt) {
    final diff = DateTime.now().difference(dt);
    if (diff.inMinutes < 1) return 'just now';
    if (diff.inMinutes < 60) return '${diff.inMinutes}m ago';
    if (diff.inHours < 24) return '${diff.inHours}h ago';
    return '${diff.inDays}d ago';
  }

  @override
  Widget build(BuildContext context) {
    final color = _color(alert.severity, context);
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        leading: Icon(_icon(alert.severity), color: color, size: 28),
        title: Text(
          alert.title,
          style: TextStyle(fontWeight: FontWeight.w600, color: color),
        ),
        subtitle: Text(alert.message),
        trailing: Text(
          _timeAgo(alert.timestamp),
          style: Theme.of(context).textTheme.labelSmall,
        ),
      ),
    );
  }
}
