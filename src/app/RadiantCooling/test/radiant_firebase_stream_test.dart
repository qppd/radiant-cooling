import 'dart:async';

import 'package:firebase_database/firebase_database.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/services/radiant_firebase.dart';

/// Counts how many `onValue` queries (in production: one RTDB connection each)
/// the service opens, so we can prove widgets share them.
class _FakeQuery implements DatabaseReference {
  _FakeQuery(this.path);

  @override
  final String path;
  int onValueReads = 0;
  final StreamController<DatabaseEvent> controller =
      StreamController<DatabaseEvent>.broadcast();

  @override
  Stream<DatabaseEvent> get onValue {
    onValueReads++;
    return controller.stream;
  }

  @override
  dynamic noSuchMethod(Invocation invocation) =>
      super.noSuchMethod(invocation);
}

class _FakeDatabase implements FirebaseDatabase {
  final Map<String, _FakeQuery> queries = {};

  _FakeQuery queryFor(String path) =>
      queries.putIfAbsent(path, () => _FakeQuery(path));

  @override
  DatabaseReference ref([String? path]) => queryFor(path ?? '/');

  @override
  dynamic noSuchMethod(Invocation invocation) =>
      super.noSuchMethod(invocation);
}

void main() {
  late _FakeDatabase db;
  late RadiantFirebase firebase;

  setUp(() {
    db = _FakeDatabase();
    firebase = RadiantFirebase(database: db);
  });

  test('repeat getters hand back the same stream, not a new query', () {
    final first = firebase.monitorStream();
    final second = firebase.monitorStream();

    expect(identical(first, second), isTrue);
    expect(db.queries['radiant/telemetry/monitor/latest']!.onValueReads, 1);
  });

  test('many listeners on one path open a single query', () {
    final subs = [
      firebase.monitorStream().listen((_) {}),
      firebase.monitorStream().listen((_) {}),
      firebase.monitorStream().listen((_) {}),
    ];

    final query = db.queries['radiant/telemetry/monitor/latest']!;
    expect(query.onValueReads, 1);
    expect(query.controller.hasListener, isTrue);

    for (final sub in subs) {
      sub.cancel();
    }
  });

  test('the whole dashboard maps to one query per distinct path', () {
    // What dashboard_screen / device_status_strip / main.dart / settings_screen
    // subscribe to across a few rebuilds.
    for (var i = 0; i < 3; i++) {
      firebase.monitorStream();
      firebase.monitorStream();
      firebase.chillerStream();
      firebase.chillerStateStream();
      firebase.dhStream();
      firebase.dhStateStream();
      firebase.heartbeatStream();
      firebase.controlParamsStream();
      firebase.dhConfigStream();
      firebase.weatherKeyStream();
    }

    expect(db.queries.keys.toSet(), {
      'radiant/telemetry/monitor/latest',
      'radiant/telemetry/chiller/latest',
      'radiant/state/chiller',
      'radiant/telemetry/dh/latest',
      'radiant/state/dh',
      'radiant/heartbeat/monitor',
      'radiant/config/control/params',
      'radiant/config/dh',
      'radiant/config/weather_key',
    });
    expect(db.queries.length, 9);
  });
}
