import 'dart:math';

double? dewPointC(double? tempC, double? humidityPct) {
  if (tempC == null || humidityPct == null) return null;
  if (humidityPct <= 0 || humidityPct > 100) return null;
  if (tempC < -90 || tempC > 60) return null;
  const a = 17.62;
  const b = 243.12;
  final alpha = (a * tempC) / (b + tempC) + log(humidityPct / 100.0);
  return (b * alpha) / (a - alpha);
}
