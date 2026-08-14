import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../services/radiant_firebase.dart';
import '../widgets/section_card.dart';

/// Settings: control parameters, dehumidifier target, system link,
/// WeatherAPI key, and sign out.
class SettingsScreen extends StatelessWidget {
  const SettingsScreen({
    super.key,
    required this.firebase,
    required this.linkedId,
    required this.weatherKey,
    required this.onLinkSystem,
    required this.onManageKey,
    required this.onSignOut,
  });

  final RadiantFirebase firebase;
  final String? linkedId;
  final String? weatherKey;
  final VoidCallback onLinkSystem;
  final VoidCallback onManageKey;
  final VoidCallback onSignOut;

  Future<void> _editControlParams(BuildContext context) async {
    final current = await firebase.controlParamsStream().first;
    if (!context.mounted) return;
    final result = await showDialog<ControlParams>(
      context: context,
      builder: (context) => _NumberDialog<ControlParams>(
        title: 'Control parameters',
        fields: [
          _NumberFieldSpec(
            label: 'Comfort setpoint (°C)',
            hint: 'Indoor temp above this triggers cooling',
            initial: current.comfortSetpointC,
            min: 16,
            max: 32,
          ),
          _NumberFieldSpec(
            label: 'Dew-point margin (°C)',
            hint: 'Water floor = dew point + margin',
            initial: current.dewpointMarginC,
            min: 0,
            max: 10,
          ),
          _NumberFieldSpec(
            label: 'Weather-cool threshold (°C)',
            hint: 'Outdoor temp must exceed this to cool',
            initial: current.weatherCoolTempC,
            min: 18,
            max: 45,
          ),
        ],
        onSave: (values) => ControlParams(
          comfortSetpointC: values[0],
          dewpointMarginC: values[1],
          weatherCoolTempC: values[2],
        ),
      ),
    );
    if (result == null) return;
    try {
      await firebase.updateControlParams(
        comfortSetpointC: result.comfortSetpointC,
        dewpointMarginC: result.dewpointMarginC,
        weatherCoolTempC: result.weatherCoolTempC,
      );
      if (context.mounted) {
        _toast(context, 'Control parameters saved');
      }
    } catch (_) {
      if (context.mounted) {
        _toast(context, 'Could not save — check the Firebase setup');
      }
    }
  }

  Future<void> _editDhConfig(BuildContext context) async {
    final current = await firebase.dhConfigStream().first;
    if (!context.mounted) return;
    final result = await showDialog<DhConfig>(
      context: context,
      builder: (context) => _NumberDialog<DhConfig>(
        title: 'Dehumidifier target',
        fields: [
          _NumberFieldSpec(
            label: 'Humidity setpoint (%)',
            hint: 'Dehumidifier runs above this RH',
            initial: current.humiditySetpointPct,
            min: 30,
            max: 80,
          ),
          _NumberFieldSpec(
            label: 'Deadband (%)',
            hint: 'Stops at setpoint − deadband',
            initial: current.humidityDeadbandPct,
            min: 0,
            max: 15,
          ),
        ],
        onSave: (values) => DhConfig(
          humiditySetpointPct: values[0],
          humidityDeadbandPct: values[1],
        ),
      ),
    );
    if (result == null) return;
    try {
      await firebase.updateDhConfig(
        humiditySetpointPct: result.humiditySetpointPct,
        humidityDeadbandPct: result.humidityDeadbandPct,
      );
      if (context.mounted) {
        _toast(context, 'Dehumidifier target saved');
      }
    } catch (_) {
      if (context.mounted) {
        _toast(context, 'Could not save — check the Firebase setup');
      }
    }
  }

  void _toast(BuildContext context, String message) {
    if (!context.mounted) return;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        SectionCard(
          title: 'Control parameters',
          icon: Icons.tune,
          trailing: TextButton(
            onPressed: () => _editControlParams(context),
            child: const Text('Edit'),
          ),
          child: StreamBuilder<ControlParams>(
            stream: firebase.controlParamsStream(),
            builder: (context, snap) {
              final p = snap.data ?? const ControlParams();
              return Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _ParamLine(
                    label: 'Comfort setpoint',
                    value: '${p.comfortSetpointC.toStringAsFixed(1)} °C',
                    sub: 'Indoor air temp above this triggers cooling',
                  ),
                  const Divider(height: 16),
                  _ParamLine(
                    label: 'Dew-point margin',
                    value: '${p.dewpointMarginC.toStringAsFixed(1)} °C',
                    sub: 'Condensation floor = ref dew point + margin',
                  ),
                  const Divider(height: 16),
                  _ParamLine(
                    label: 'Weather-cool threshold',
                    value: '${p.weatherCoolTempC.toStringAsFixed(1)} °C',
                    sub: 'Outdoor temp must exceed this to cool',
                  ),
                ],
              );
            },
          ),
        ),
        const SizedBox(height: 12),
        SectionCard(
          title: 'Dehumidifier target',
          icon: Icons.water_drop_outlined,
          trailing: TextButton(
            onPressed: () => _editDhConfig(context),
            child: const Text('Edit'),
          ),
          child: StreamBuilder<DhConfig>(
            stream: firebase.dhConfigStream(),
            builder: (context, snap) {
              final c = snap.data ?? const DhConfig();
              return Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _ParamLine(
                    label: 'Humidity setpoint',
                    value: '${c.humiditySetpointPct.toStringAsFixed(0)} %',
                    sub: 'Dehumidifier runs above this RH',
                  ),
                  const Divider(height: 16),
                  _ParamLine(
                    label: 'Deadband',
                    value: '${c.humidityDeadbandPct.toStringAsFixed(0)} %',
                    sub: 'Stops at setpoint minus deadband',
                  ),
                ],
              );
            },
          ),
        ),
        const SizedBox(height: 12),
        Card(
          child: ListTile(
            leading: Icon(linkedId == null ? Icons.link_off : Icons.link),
            title: Text(linkedId == null ? 'Not linked' : linkedId!),
            subtitle: const Text(
              'System ID connecting this app to the ESP32 gateway '
              'via Firebase',
            ),
            trailing: TextButton(
              onPressed: onLinkSystem,
              child: Text(linkedId == null ? 'Link' : 'Change'),
            ),
          ),
        ),
        const SizedBox(height: 12),
        Card(
          child: ListTile(
            leading: Icon(weatherKey == null ? Icons.key_off : Icons.key),
            title: Text(
              weatherKey == null ? 'No WeatherAPI key' : 'WeatherAPI key set',
            ),
            subtitle: const Text(
              'Managed from this app and delivered to the gateway '
              '(radiant/config/weather_key). The ESP32 fetches the '
              'weather itself.',
            ),
            trailing: TextButton(
              onPressed: onManageKey,
              child: Text(weatherKey == null ? 'Set key' : 'Change'),
            ),
          ),
        ),
        const SizedBox(height: 20),
        OutlinedButton.icon(
          onPressed: onSignOut,
          icon: const Icon(Icons.logout),
          label: const Text('Sign out'),
        ),
      ],
    );
  }
}

class _ParamLine extends StatelessWidget {
  const _ParamLine({
    required this.label,
    required this.value,
    required this.sub,
  });

  final String label;
  final String value;
  final String sub;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(label),
              const SizedBox(height: 2),
              Text(
                sub,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: theme.colorScheme.onSurfaceVariant,
                ),
              ),
            ],
          ),
        ),
        const SizedBox(width: 12),
        Text(
          value,
          style: theme.textTheme.titleMedium?.copyWith(
            fontWeight: FontWeight.bold,
          ),
        ),
      ],
    );
  }
}

class _NumberFieldSpec {
  const _NumberFieldSpec({
    required this.label,
    required this.hint,
    required this.initial,
    required this.min,
    required this.max,
  });

  final String label;
  final String hint;
  final double initial;
  final double min;
  final double max;
}

/// A dialog of numeric fields that validates each value against its range
/// and returns the parsed result via `onSave`.
class _NumberDialog<T> extends StatefulWidget {
  const _NumberDialog({
    required this.title,
    required this.fields,
    required this.onSave,
  });

  final String title;
  final List<_NumberFieldSpec> fields;
  final T Function(List<double> values) onSave;

  @override
  State<_NumberDialog<T>> createState() => _NumberDialogState<T>();
}

class _NumberDialogState<T> extends State<_NumberDialog<T>> {
  final _formKey = GlobalKey<FormState>();
  late final List<TextEditingController> _controllers;

  @override
  void initState() {
    super.initState();
    _controllers = [
      for (final f in widget.fields)
        TextEditingController(text: f.initial.toStringAsFixed(1)),
    ];
  }

  @override
  void dispose() {
    for (final c in _controllers) {
      c.dispose();
    }
    super.dispose();
  }

  String? _validate(int i, String? value) {
    final v = double.tryParse(value ?? '');
    final spec = widget.fields[i];
    if (v == null) return 'Enter a number';
    if (v < spec.min || v > spec.max) {
      return 'Between ${spec.min.toStringAsFixed(0)} and '
          '${spec.max.toStringAsFixed(0)}';
    }
    return null;
  }

  void _submit() {
    if (!_formKey.currentState!.validate()) return;
    Navigator.pop(
      context,
      widget.onSave([for (final c in _controllers) double.parse(c.text)]),
    );
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text(widget.title),
      content: SingleChildScrollView(
        child: Form(
          key: _formKey,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              for (var i = 0; i < widget.fields.length; i++) ...[
                if (i > 0) const SizedBox(height: 12),
                TextFormField(
                  controller: _controllers[i],
                  keyboardType: const TextInputType.numberWithOptions(
                    decimal: true,
                  ),
                  inputFormatters: [
                    FilteringTextInputFormatter.allow(RegExp(r'[0-9.\-]')),
                  ],
                  decoration: InputDecoration(
                    labelText: widget.fields[i].label,
                    helperText: widget.fields[i].hint,
                    border: const OutlineInputBorder(),
                  ),
                  validator: (v) => _validate(i, v),
                  onFieldSubmitted: (_) {
                    if (i == widget.fields.length - 1) _submit();
                  },
                ),
              ],
            ],
          ),
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
        FilledButton(onPressed: _submit, child: const Text('Save')),
      ],
    );
  }
}
