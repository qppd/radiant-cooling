import 'package:flutter/material.dart';

import '../services/notification_service.dart';
import '../widgets/section_card.dart';

class NotificationSettingsScreen extends StatefulWidget {
  const NotificationSettingsScreen({super.key, required this.service});

  final NotificationService service;

  @override
  State<NotificationSettingsScreen> createState() =>
      _NotificationSettingsScreenState();
}

class _NotificationSettingsScreenState
    extends State<NotificationSettingsScreen> {
  NotificationPrefs _prefs = const NotificationPrefs();
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final prefs = await widget.service.load();
    if (!mounted) return;
    setState(() {
      _prefs = prefs;
      _loading = false;
    });
  }

  Future<void> _save(NotificationPrefs prefs) async {
    setState(() => _prefs = prefs);
    await widget.service.save(prefs);
  }

  @override
  Widget build(BuildContext context) {
    if (_loading) {
      return const Scaffold(body: Center(child: CircularProgressIndicator()));
    }

    return Scaffold(
      appBar: AppBar(title: const Text('Notification settings')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          SwitchListTile(
            title: const Text('Enable notifications'),
            subtitle: const Text('Master toggle for all alerts'),
            value: _prefs.allEnabled,
            onChanged: (v) => _save(_prefs.copyWith(allEnabled: v)),
          ),
          const Divider(),

          SectionCard(
            title: 'Alert types',
            icon: Icons.notifications_outlined,
            child: Column(
              children: [
                _toggle('Gateway offline', 'Critical alerts when gateway disconnects', _prefs.gatewayOffline,
                    (v) => _save(_prefs.copyWith(gatewayOffline: v))),
                _toggle('Sensor failure', 'When a temperature sensor returns invalid data', _prefs.sensorFailure,
                    (v) => _save(_prefs.copyWith(sensorFailure: v))),
                _toggle('Condensation risk', 'When pipe temp drops below the water floor', _prefs.condensationRisk,
                    (v) => _save(_prefs.copyWith(condensationRisk: v))),
                _toggle('Weather stale', 'When outdoor weather data becomes unavailable', _prefs.weatherStale,
                    (v) => _save(_prefs.copyWith(weatherStale: v))),
                _toggle('Pump state changes', 'When water pumps turn on or off', _prefs.pumpsState,
                    (v) => _save(_prefs.copyWith(pumpsState: v))),
                _toggle('Dehumidifier state', 'When the dehumidifier turns on or off', _prefs.dehumidifierState,
                    (v) => _save(_prefs.copyWith(dehumidifierState: v))),
              ],
            ),
          ),
          const SizedBox(height: 12),

          SectionCard(
            title: 'Quiet hours',
            icon: Icons.do_not_disturb_on_outlined,
            child: Column(
              children: [
                ListTile(
                  contentPadding: EdgeInsets.zero,
                  title: const Text('Do not disturb'),
                  subtitle: Text(
                    'From ${_hourLabel(_prefs.quietStartHour)} '
                    'to ${_hourLabel(_prefs.quietEndHour)}',
                  ),
                  trailing: const Icon(Icons.chevron_right),
                  onTap: () => _pickQuietHours(),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _toggle(
    String title,
    String subtitle,
    bool value,
    ValueChanged<bool> onChanged,
  ) {
    return SwitchListTile(
      contentPadding: EdgeInsets.zero,
      title: Text(title),
      subtitle: Text(subtitle),
      value: value && _prefs.allEnabled,
      onChanged: _prefs.allEnabled ? onChanged : null,
    );
  }

  String _hourLabel(int hour) {
    final h = hour % 12 == 0 ? 12 : hour % 12;
    final suffix = hour < 12 ? 'AM' : 'PM';
    return '$h:00 $suffix';
  }

  Future<void> _pickQuietHours() async {
    TimeOfDay start = TimeOfDay(hour: _prefs.quietStartHour, minute: 0);
    TimeOfDay end = TimeOfDay(hour: _prefs.quietEndHour, minute: 0);

    final pickedStart = await showTimePicker(
      context: context,
      initialTime: start,
      helpText: 'Quiet hours start',
    );
    if (pickedStart == null || !mounted) return;

    final pickedEnd = await showTimePicker(
      context: context,
      initialTime: end,
      helpText: 'Quiet hours end',
    );
    if (pickedEnd == null) return;

    await _save(_prefs.copyWith(
      quietStartHour: pickedStart.hour,
      quietEndHour: pickedEnd.hour,
    ));
  }
}
