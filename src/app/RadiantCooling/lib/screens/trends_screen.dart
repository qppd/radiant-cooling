import 'dart:math';

import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';

import '../models/telemetry.dart';
import '../services/radiant_firebase.dart';
import '../services/telemetry_logger.dart';
import '../widgets/section_card.dart';

class TrendsScreen extends StatefulWidget {
  const TrendsScreen({
    super.key,
    required this.firebase,
    required this.logger,
  });

  final RadiantFirebase firebase;
  final TelemetryLogger logger;

  @override
  State<TrendsScreen> createState() => _TrendsScreenState();
}

class _TrendsScreenState extends State<TrendsScreen> {
  Duration _window = const Duration(hours: 6);
  List<TelemetryPoint> _points = const [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final points = await widget.logger.loadWindow(_window);
    if (!mounted) return;
    setState(() {
      _points = points;
      _loading = false;
    });
  }

  void _setWindow(Duration d) {
    setState(() => _window = d);
    _load();
  }

  @override
  Widget build(BuildContext context) {
    if (_loading) {
      return const Center(child: CircularProgressIndicator());
    }

    if (_points.isEmpty) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(
              Icons.show_chart,
              size: 56,
              color: Theme.of(context).colorScheme.outline,
            ),
            const SizedBox(height: 12),
            const Text('No data yet'),
            const SizedBox(height: 4),
            Text(
              'Sensor readings will appear here as the system runs.',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
        ),
      );
    }

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        SegmentedButton<Duration>(
          segments: const [
            ButtonSegment(value: Duration(hours: 1), label: Text('1h')),
            ButtonSegment(value: Duration(hours: 6), label: Text('6h')),
            ButtonSegment(value: Duration(hours: 24), label: Text('24h')),
            ButtonSegment(
              value: Duration(days: 7),
              label: Text('7d'),
            ),
          ],
          selected: {_window},
          onSelectionChanged: (s) => _setWindow(s.first),
        ),
        const SizedBox(height: 16),

        SectionCard(
          title: 'Pipe temperatures',
          icon: Icons.thermostat,
          child: SizedBox(
            height: 200,
            child: _LineChart(
              series: [
                _Series(
                  label: 'Supply',
                  color: Colors.blue,
                  values: _points.map((p) => p.supplyC).toList(),
                ),
                _Series(
                  label: 'Return',
                  color: Colors.orange,
                  values: _points.map((p) => p.returnC).toList(),
                ),
                _Series(
                  label: 'Tank',
                  color: Colors.green,
                  values: _points.map((p) => p.tankTempC).toList(),
                ),
                _Series(
                  label: 'Coldest',
                  color: Colors.red,
                  values: _points.map((p) => p.coldestPipeC).toList(),
                ),
              ],
              timestamps: _points.map((p) => p.timestamp).toList(),
            ),
          ),
        ),
        const SizedBox(height: 12),

        SectionCard(
          title: 'Indoor climate',
          icon: Icons.home_outlined,
          child: Column(
            children: [
              SizedBox(
                height: 180,
                child: _LineChart(
                  series: [
                    _Series(
                      label: 'Temp (°C)',
                      color: Colors.blue,
                      values: _points.map((p) => p.indoorTempC).toList(),
                    ),
                  ],
                  timestamps: _points.map((p) => p.timestamp).toList(),
                ),
              ),
              const SizedBox(height: 8),
              SizedBox(
                height: 180,
                child: _LineChart(
                  series: [
                    _Series(
                      label: 'Humidity (%)',
                      color: Colors.green,
                      values: _points.map((p) => p.indoorHumidityPct).toList(),
                    ),
                  ],
                  timestamps: _points.map((p) => p.timestamp).toList(),
                  yLabel: '%',
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),

        SectionCard(
          title: 'Outdoor weather',
          icon: Icons.wb_sunny_outlined,
          child: SizedBox(
            height: 200,
            child: _LineChart(
              series: [
                _Series(
                  label: 'Temp',
                  color: Colors.orange,
                  values: _points.map((p) => p.outdoorTempC).toList(),
                ),
                _Series(
                  label: 'Dew point',
                  color: Colors.purple,
                  values: _points.map((p) => p.outdoorDewPointC).toList(),
                ),
              ],
              timestamps: _points.map((p) => p.timestamp).toList(),
            ),
          ),
        ),
        const SizedBox(height: 12),

        _SummaryStats(points: _points),
      ],
    );
  }
}


class _Series {
  const _Series({
    required this.label,
    required this.color,
    required this.values,
  });
  final String label;
  final Color color;
  final List<double?> values;
}

class _LineChart extends StatelessWidget {
  const _LineChart({
    required this.series,
    required this.timestamps,
    this.yLabel = '°C',
  });

  final List<_Series> series;
  final List<DateTime> timestamps;
  final String yLabel;

  @override
  Widget build(BuildContext context) {
    double yMin = double.infinity;
    double yMax = double.negativeInfinity;
    for (final s in series) {
      for (final v in s.values) {
        if (v == null) continue;
        if (v < yMin) yMin = v;
        if (v > yMax) yMax = v;
      }
    }
    if (!yMin.isFinite) {
      yMin = 0;
      yMax = 100;
    }
    final yPad = max((yMax - yMin) * 0.1, 1.0);

    return LineChart(
      LineChartData(
        gridData: FlGridData(
          show: true,
          drawVerticalLine: false,
          horizontalInterval: max((yMax - yMin + yPad * 2) / 4, 1),
        ),
        titlesData: FlTitlesData(
          leftTitles: AxisTitles(
            sideTitles: SideTitles(
              showTitles: true,
              reservedSize: 40,
              getTitlesWidget: (v, _) => Text(
                v.toStringAsFixed(0),
                style: const TextStyle(fontSize: 10),
              ),
            ),
          ),
          bottomTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          topTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
          rightTitles: const AxisTitles(
            sideTitles: SideTitles(showTitles: false),
          ),
        ),
        borderData: FlBorderData(show: false),
        minY: yMin - yPad,
        maxY: yMax + yPad,
        lineBarsData: [
          for (final s in series)
            LineChartBarData(
              spots: [
                for (var i = 0; i < s.values.length; i++)
                  if (s.values[i] != null)
                    FlSpot(i.toDouble(), s.values[i]!),
              ],
              isCurved: true,
              color: s.color,
              barWidth: 2,
              dotData: const FlDotData(show: false),
              belowBarData: BarAreaData(
                show: true,
                color: s.color.withValues(alpha: 0.08),
              ),
            ),
        ],
        lineTouchData: LineTouchData(
          touchTooltipData: LineTouchTooltipData(
            getTooltipItems: (spots) => [
              for (final spot in spots)
                LineTooltipItem(
                  '${spot.y.toStringAsFixed(1)}$yLabel',
                  TextStyle(
                    color: series[spot.barIndex % series.length].color,
                    fontSize: 12,
                    fontWeight: FontWeight.w600,
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }
}

class _SummaryStats extends StatelessWidget {
  const _SummaryStats({required this.points});
  final List<TelemetryPoint> points;

  @override
  Widget build(BuildContext context) {
    if (points.isEmpty) return const SizedBox.shrink();

    double? minVal, maxVal, sumVal;
    int count = 0;
    for (final p in points) {
      final v = p.supplyC;
      if (v == null) continue;
      if (minVal == null || v < minVal) minVal = v;
      if (maxVal == null || v > maxVal) maxVal = v;
      sumVal = (sumVal ?? 0) + v;
      count++;
    }

    if (count == 0) return const SizedBox.shrink();
    final avg = sumVal! / count;

    return SectionCard(
      title: 'Supply temperature summary',
      icon: Icons.analytics_outlined,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceAround,
        children: [
          _Stat(label: 'Min', value: '${minVal!.toStringAsFixed(1)} °C'),
          _Stat(label: 'Avg', value: '${avg.toStringAsFixed(1)} °C'),
          _Stat(label: 'Max', value: '${maxVal!.toStringAsFixed(1)} °C'),
          _Stat(label: 'Points', value: '$count'),
        ],
      ),
    );
  }
}

class _Stat extends StatelessWidget {
  const _Stat({required this.label, required this.value});
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Column(
      children: [
        Text(label, style: theme.textTheme.bodySmall),
        const SizedBox(height: 2),
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
