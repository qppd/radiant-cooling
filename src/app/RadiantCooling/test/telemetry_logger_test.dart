import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/services/telemetry_logger.dart';

void main() {
  late File tempFile;
  late TelemetryLogger logger;

  setUp(() {
    tempFile = File('${Directory.systemTemp.path}/test_telemetry_${DateTime.now().millisecondsSinceEpoch}.json');
    logger = TelemetryLogger(file: tempFile);
  });

  tearDown(() async {
    if (await tempFile.exists()) await tempFile.delete();
  });

  test('load returns empty list when no file exists', () async {
    final points = await logger.load();
    expect(points, isEmpty);
  });

  test('log persists and load returns points', () async {
    final now = DateTime.now();
    await logger.log(TelemetryPoint(
      timestamp: now,
      supplyC: 15.0,
      returnC: 16.5,
      indoorTempC: 25.0,
    ));

    final points = await logger.load();
    expect(points, hasLength(1));
    expect(points.first.supplyC, 15.0);
    expect(points.first.returnC, 16.5);
    expect(points.first.indoorTempC, 25.0);
  });

  test('loadWindow filters by time range', () async {
    final now = DateTime.now();
    await logger.log(TelemetryPoint(
      timestamp: now.subtract(const Duration(minutes: 30)),
      supplyC: 15.0,
    ));
    await logger.log(TelemetryPoint(
      timestamp: now.subtract(const Duration(hours: 2)),
      supplyC: 16.0,
    ));

    final window = await logger.loadWindow(const Duration(hours: 1));
    expect(window, hasLength(1));
    expect(window.first.supplyC, 15.0);

    final all = await logger.loadWindow(const Duration(hours: 3));
    expect(all, hasLength(2));
  });

  test('clear removes all data', () async {
    await logger.log(TelemetryPoint(
      timestamp: DateTime.now(),
      supplyC: 15.0,
    ));
    expect(await logger.load(), hasLength(1));

    await logger.clear();
    expect(await logger.load(), isEmpty);
  });

  test('multiple log calls append points', () async {
    final now = DateTime.now();
    for (var i = 0; i < 5; i++) {
      await logger.log(TelemetryPoint(
        timestamp: now.subtract(Duration(minutes: i)),
        supplyC: 15.0 + i,
      ));
    }

    final points = await logger.load();
    expect(points, hasLength(5));
  });
}
