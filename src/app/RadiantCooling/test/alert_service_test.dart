import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/models/alert.dart';
import 'package:radiant_cooling/services/alert_service.dart';

void main() {
  late File tempFile;
  late AlertService service;

  setUp(() {
    tempFile = File(
      '${Directory.systemTemp.path}/test_alerts_'
      '${DateTime.now().microsecondsSinceEpoch}.json',
    );
    service = AlertService(file: tempFile);
  });

  tearDown(() async {
    if (await tempFile.exists()) await tempFile.delete();
  });

  Alert newAlert({DateTime? at}) => Alert(
        type: AlertType.condensationRisk,
        severity: AlertSeverity.warning,
        title: 'Condensation risk',
        message: 'Coldest pipe is below the water floor.',
        timestamp: at ?? DateTime.now(),
      );

  test('addAlert works when no alert file exists yet (fresh install)',
      () async {
    expect(await service.load(), isEmpty);

    await service.addAlert(newAlert());

    final loaded = await service.load();
    expect(loaded, hasLength(1));
    expect(loaded.first.title, 'Condensation risk');
  });

  test('appends across successive alerts and returns newest first', () async {
    await service.addAlert(newAlert(at: DateTime.now().subtract(const Duration(minutes: 5))));
    await service.addAlert(newAlert(at: DateTime.now()));

    final loaded = await service.load();
    expect(loaded, hasLength(2));
    expect(
      loaded.first.timestamp.isAfter(loaded.last.timestamp),
      isTrue,
    );
  });

  test('drops alerts older than the retention window on write', () async {
    await service.addAlert(
      newAlert(at: DateTime.now().subtract(const Duration(days: 8))),
    );
    await service.addAlert(newAlert());

    final loaded = await service.load();
    expect(loaded, hasLength(1));
    expect(
      loaded.first.timestamp.isAfter(
        DateTime.now().subtract(const Duration(days: 1)),
      ),
      isTrue,
    );
  });
}
