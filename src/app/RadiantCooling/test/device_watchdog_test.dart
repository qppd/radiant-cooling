import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/models/telemetry.dart';
import 'package:radiant_cooling/services/device_watchdog.dart';

import 'fakes.dart';

void main() {
  late FakeRadiantFirebase firebase;
  late DateTime now;
  late DeviceWatchdog watchdog;

  setUp(() {
    firebase = FakeRadiantFirebase();
    now = DateTime(2026, 3, 1, 9, 0, 0);
    watchdog = DeviceWatchdog(
      firebase: firebase,
      systemId: 'RADIANT-001',
      staleAfter: const Duration(seconds: 90),
      clock: () => now,
    );
  });

  test('leaves a device alone while it is reporting', () async {
    watchdog.touch(DeviceWatchdog.chillerId);
    await watchdog.sweep();

    expect(firebase.deviceOnlineWrites, isEmpty);
  });

  test('never marks a device it has not heard from', () async {
    await watchdog.sweep();

    expect(firebase.deviceOnlineWrites, isEmpty);
  });

  test('marks a silent peer offline once, then back online when it returns',
      () async {
    watchdog.touch(DeviceWatchdog.chillerId);

    now = now.add(const Duration(seconds: 91));
    await watchdog.sweep();

    final offline = firebase.deviceOnlineWrites.single;
    expect(offline.device, DeviceWatchdog.chillerId);
    expect(offline.online, isFalse);
    // Peer entries are tagged so they never show up as linkable systems.
    expect(offline.role, 'peer');
    expect(offline.parent, 'RADIANT-001');
    expect(offline.lastSeenTs, isNotNull);

    // Still silent: no repeated writes, only the transition.
    now = now.add(const Duration(minutes: 10));
    await watchdog.sweep();
    expect(firebase.deviceOnlineWrites, hasLength(1));

    watchdog.touch(DeviceWatchdog.chillerId, at: now);
    await watchdog.sweep();

    expect(firebase.deviceOnlineWrites, hasLength(2));
    expect(firebase.deviceOnlineWrites.last.online, isTrue);
  });

  test('marks the gateway itself offline with no peer metadata', () async {
    watchdog.touch(watchdog.monitorId);

    now = now.add(const Duration(seconds: 91));
    await watchdog.sweep();

    final write = firebase.deviceOnlineWrites.single;
    expect(write.device, 'RADIANT-001');
    expect(write.online, isFalse);
    expect(write.role, isNull);
    expect(write.parent, isNull);
  });

  test('tracks the three boards independently', () async {
    watchdog.touch(watchdog.monitorId);
    watchdog.touch(DeviceWatchdog.dehumidifierId);

    now = now.add(const Duration(seconds: 91));
    watchdog.touch(DeviceWatchdog.chillerId, at: now);
    await watchdog.sweep();

    final offlineDevices = [
      for (final w in firebase.deviceOnlineWrites)
        if (!w.online) w.device,
    ];
    expect(offlineDevices, unorderedEquals(['RADIANT-001', 'dh']));
  });

  test('start() watches the device streams and dispose() stops it', () async {
    final live = FakeRadiantFirebase(
      heartbeat: const Heartbeat(online: true, deviceId: 'RADIANT-001'),
      chiller: const ChillerTelemetry(waterTempC: 12.4),
      dh: const DhTelemetry(tempC: 26, humidityPct: 55),
    );
    final wd = DeviceWatchdog(
      firebase: live,
      systemId: 'RADIANT-001',
      staleAfter: const Duration(seconds: 90),
      clock: () => now,
    );

    wd.start();
    await Future<void>.delayed(const Duration(milliseconds: 5));

    // All three boards reported, so once they go quiet they are all marked.
    now = now.add(const Duration(seconds: 91));
    await wd.sweep();
    expect(live.deviceOnlineWrites, hasLength(3));

    wd.dispose();
    now = now.add(const Duration(minutes: 10));
    await wd.sweep();
    expect(live.deviceOnlineWrites, hasLength(3));
  });
}
