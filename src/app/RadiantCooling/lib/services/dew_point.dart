import 'dart:math';

/// Magnus-formula dew point (°C), mirroring the gateway's
/// `ClimateControl::dewPointC` so the app computes the same values live
/// from the Firebase telemetry streams.
///
/// Returns null when the inputs are invalid (missing, out-of-range
/// humidity, or a temperature outside the valid window).
double? dewPointC(double? tempC, double? humidityPct) {
  if (tempC == null || humidityPct == null) return null;
  if (humidityPct <= 0 || humidityPct > 100) return null;
  if (tempC < -90 || tempC > 60) return null;
  const a = 17.62;
  const b = 243.12;
  final alpha = (a * tempC) / (b + tempC) + log(humidityPct / 100.0);
  return (b * alpha) / (a - alpha);
}
