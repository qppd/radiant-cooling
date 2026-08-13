import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/services/device_link.dart';
import 'package:radiant_cooling/services/weather_key_store.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('DeviceLink persists and loads the linked system ID', () async {
    SharedPreferences.setMockInitialValues({});

    final link = DeviceLink();
    expect(await link.load(), isNull);

    await link.save('RADIANT-001');
    expect(await link.load(), 'RADIANT-001');

    // Values are trimmed on save.
    await link.save('  RADIANT-002  ');
    expect(await link.load(), 'RADIANT-002');

    await link.clear();
    expect(await link.load(), isNull);
  });

  test('WeatherKeyStore persists and loads the API key', () async {
    SharedPreferences.setMockInitialValues({});

    final store = WeatherKeyStore();
    expect(await store.load(), isNull);

    await store.save('  abc123def456  ');
    expect(await store.load(), 'abc123def456');

    await store.clear();
    expect(await store.load(), isNull);
  });
}
