import 'dart:async';

import 'package:flutter/foundation.dart';

import 'radiant_firebase.dart';

/// Marks a device's registry node offline when its updates stop.
///
/// A gateway can publish "I am online", but nothing can publish "I just died",
/// so the app — which already streams every device's data — watches freshness
/// and writes `radiant/devices/<device>/online = false` once a device goes
/// quiet. Only transitions are written, so while a device is alive its own
/// heartbeat keeps owning its status.
class DeviceWatchdog {
  DeviceWatchdog({
    required this.firebase,
    required this.systemId,
    this.staleAfter = const Duration(seconds: 90),
    this.sweepInterval = const Duration(seconds: 15),
    DateTime Function()? clock,
  }) : _clock = clock ?? DateTime.now;

  final RadiantFirebase firebase;
  final String systemId;

  /// How long a device may be silent before it is marked offline. Every board
  /// reports on a 30 s cycle, so 90 s tolerates two lost cycles.
  final Duration staleAfter;

  final Duration sweepInterval;
  final DateTime Function() _clock;

  static const String chillerId = 'chiller';
  static const String dehumidifierId = 'dh';

  /// Peers write their registry entry with `role: 'peer'` and a parent system,
  /// so they never show up as linkable systems in the app.
  static const Set<String> _peerIds = {chillerId, dehumidifierId};

  /// Registry id of the gateway itself.
  String get monitorId => systemId;

  /// device id -> when we last saw data from it (app clock).
  final Map<String, DateTime> _lastSeen = {};
  final Set<String> _markedOffline = {};
  final List<StreamSubscription<dynamic>> _subs = [];
  Timer? _timer;
  bool _disposed = false;

  /// Watch the three device streams and sweep on [sweepInterval].
  void start() {
    _subs.add(firebase.heartbeatStream().listen((_) => touch(monitorId)));
    _subs.add(firebase.chillerStream().listen((_) => touch(chillerId)));
    _subs.add(firebase.dhStream().listen((_) => touch(dehumidifierId)));
    _timer = Timer.periodic(sweepInterval, (_) => sweep());
  }

  /// Record data from [device] arriving now.
  void touch(String device, {DateTime? at}) {
    if (_disposed) return;
    _lastSeen[device] = at ?? _clock();
  }

  /// Flip silent devices offline and bring back any that reported again.
  ///
  /// A device we have never heard from is left alone: no data is not the same
  /// as a dead device, and the registry node may not exist yet.
  Future<void> sweep() async {
    if (_disposed) return;
    final now = _clock();
    for (final entry in _lastSeen.entries.toList()) {
      final device = entry.key;
      final silent = now.difference(entry.value) > staleAfter;
      if (silent && !_markedOffline.contains(device)) {
        _markedOffline.add(device);
        await _write(device, online: false, lastSeen: entry.value);
      } else if (!silent && _markedOffline.remove(device)) {
        await _write(device, online: true);
      }
    }
  }

  Future<void> _write(
    String device, {
    required bool online,
    DateTime? lastSeen,
  }) async {
    try {
      await firebase.setDeviceOnline(
        device,
        online: online,
        role: _peerIds.contains(device) ? 'peer' : null,
        parent: _peerIds.contains(device) ? systemId : null,
        lastSeenTs: lastSeen == null
            ? null
            : lastSeen.millisecondsSinceEpoch ~/ 1000,
      );
    } catch (error) {
      // A rules denial must not take the app down; the device's own heartbeat
      // stays the primary source of truth either way.
      debugPrint(
        '[watchdog] could not mark $device '
        '${online ? 'online' : 'offline'}: $error',
      );
    }
  }

  void dispose() {
    _disposed = true;
    _timer?.cancel();
    _timer = null;
    for (final sub in _subs) {
      sub.cancel();
    }
    _subs.clear();
    _lastSeen.clear();
    _markedOffline.clear();
  }
}
