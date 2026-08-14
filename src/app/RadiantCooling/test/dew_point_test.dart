import 'package:flutter_test/flutter_test.dart';
import 'package:radiant_cooling/services/dew_point.dart';

void main() {
  test('dewPointC mirrors the gateway Magnus formula', () {
    // Spot checks from ClimateControl.cpp test cases (~16.3 at 26 °C / 55%).
    final dp = dewPointC(26.0, 55.0);
    expect(dp, isNotNull);
    expect(dp!, closeTo(16.3, 0.3));

    // 20 °C / 60% RH.
    expect(dewPointC(20.0, 60.0), closeTo(12.0, 0.5));
  });

  test('dewPointC returns null for invalid inputs', () {
    expect(dewPointC(null, 50.0), isNull);
    expect(dewPointC(20.0, null), isNull);
    expect(dewPointC(20.0, 0), isNull);
    expect(dewPointC(20.0, 101), isNull);
    expect(dewPointC(-100.0, 50.0), isNull); // stale-outdoor sentinel
    expect(dewPointC(85.0, 50.0), isNull); // invalid sensor glitch
  });
}
