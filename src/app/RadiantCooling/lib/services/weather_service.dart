import 'dart:convert';

import 'package:http/http.dart' as http;

import '../config/app_config.dart';

/// Outdoor conditions from WeatherAPI.com.
class WeatherConditions {
  const WeatherConditions({
    required this.ok,
    this.tempC = 0,
    this.dewPointC = 0,
    this.humidityPct = 0,
  });

  /// False when the fetch failed or returned a non-200 response.
  final bool ok;
  final double tempC;
  final double dewPointC;
  final double humidityPct;
}

/// Calls the WeatherAPI.com current-weather endpoint.
///
/// The API key is read from [AppConfig] (the app owns the key — the ESP32
/// never calls this API). The caller publishes the result to Firebase so
/// the gateway can stream it.
class WeatherService {
  Future<WeatherConditions> fetchCurrent() async {
    final uri = Uri.parse('https://api.weatherapi.com/v1/current.json')
        .replace(queryParameters: {
      'key': AppConfig.weatherApiKey,
      'q': AppConfig.weatherLocation,
      'aqi': 'no',
    });

    try {
      final res = await http.get(uri).timeout(const Duration(seconds: 10));
      if (res.statusCode != 200) return const WeatherConditions(ok: false);

      final json = jsonDecode(res.body) as Map<String, dynamic>;
      final cur = json['current'] as Map<String, dynamic>? ?? const {};
      return WeatherConditions(
        ok: true,
        tempC: (cur['temp_c'] as num?)?.toDouble() ?? 0,
        dewPointC: (cur['dewpoint_c'] as num?)?.toDouble() ?? 0,
        humidityPct: (cur['humidity'] as num?)?.toDouble() ?? 0,
      );
    } catch (_) {
      return const WeatherConditions(ok: false);
    }
  }
}
