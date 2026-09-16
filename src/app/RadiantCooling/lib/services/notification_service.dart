import 'package:shared_preferences/shared_preferences.dart';

class NotificationService {
  static const _prefix = 'notif_';

  static const _allEnabled = '${_prefix}all_enabled';
  static const _gatewayOffline = '${_prefix}gateway_offline';
  static const _sensorFailure = '${_prefix}sensor_failure';
  static const _condensationRisk = '${_prefix}condensation_risk';
  static const _weatherStale = '${_prefix}weather_stale';
  static const _pumpsState = '${_prefix}pumps_state';
  static const _dehumidifierState = '${_prefix}dehumidifier_state';

  static const _quietStart = '${_prefix}quiet_start';
  static const _quietEnd = '${_prefix}quiet_end';

  Future<NotificationPrefs> load() async {
    final prefs = await SharedPreferences.getInstance();
    return NotificationPrefs(
      allEnabled: prefs.getBool(_allEnabled) ?? true,
      gatewayOffline: prefs.getBool(_gatewayOffline) ?? true,
      sensorFailure: prefs.getBool(_sensorFailure) ?? true,
      condensationRisk: prefs.getBool(_condensationRisk) ?? true,
      weatherStale: prefs.getBool(_weatherStale) ?? true,
      pumpsState: prefs.getBool(_pumpsState) ?? false,
      dehumidifierState: prefs.getBool(_dehumidifierState) ?? false,
      quietStartHour: prefs.getInt(_quietStart) ?? 22,
      quietEndHour: prefs.getInt(_quietEnd) ?? 7,
    );
  }

  Future<void> save(NotificationPrefs prefs) async {
    final sp = await SharedPreferences.getInstance();
    await sp.setBool(_allEnabled, prefs.allEnabled);
    await sp.setBool(_gatewayOffline, prefs.gatewayOffline);
    await sp.setBool(_sensorFailure, prefs.sensorFailure);
    await sp.setBool(_condensationRisk, prefs.condensationRisk);
    await sp.setBool(_weatherStale, prefs.weatherStale);
    await sp.setBool(_pumpsState, prefs.pumpsState);
    await sp.setBool(_dehumidifierState, prefs.dehumidifierState);
    await sp.setInt(_quietStart, prefs.quietStartHour);
    await sp.setInt(_quietEnd, prefs.quietEndHour);
  }

  bool shouldNotify(NotificationPrefs prefs, String alertType) {
    if (!prefs.allEnabled) return false;
    final hour = DateTime.now().hour;
    if (prefs.quietStartHour != prefs.quietEndHour) {
      if (prefs.quietStartHour < prefs.quietEndHour) {
        if (hour >= prefs.quietStartHour && hour < prefs.quietEndHour) {
          return false;
        }
      } else {
        if (hour >= prefs.quietStartHour || hour < prefs.quietEndHour) {
          return false;
        }
      }
    }
    switch (alertType) {
      case 'gatewayOffline':
        return prefs.gatewayOffline;
      case 'sensorFailure':
        return prefs.sensorFailure;
      case 'condensationRisk':
        return prefs.condensationRisk;
      case 'weatherStale':
        return prefs.weatherStale;
      case 'pumpsState':
        return prefs.pumpsState;
      case 'dehumidifierState':
        return prefs.dehumidifierState;
      default:
        return false;
    }
  }
}

class NotificationPrefs {
  const NotificationPrefs({
    this.allEnabled = true,
    this.gatewayOffline = true,
    this.sensorFailure = true,
    this.condensationRisk = true,
    this.weatherStale = true,
    this.pumpsState = false,
    this.dehumidifierState = false,
    this.quietStartHour = 22,
    this.quietEndHour = 7,
  });

  final bool allEnabled;
  final bool gatewayOffline;
  final bool sensorFailure;
  final bool condensationRisk;
  final bool weatherStale;
  final bool pumpsState;
  final bool dehumidifierState;
  final int quietStartHour;
  final int quietEndHour;

  NotificationPrefs copyWith({
    bool? allEnabled,
    bool? gatewayOffline,
    bool? sensorFailure,
    bool? condensationRisk,
    bool? weatherStale,
    bool? pumpsState,
    bool? dehumidifierState,
    int? quietStartHour,
    int? quietEndHour,
  }) => NotificationPrefs(
    allEnabled: allEnabled ?? this.allEnabled,
    gatewayOffline: gatewayOffline ?? this.gatewayOffline,
    sensorFailure: sensorFailure ?? this.sensorFailure,
    condensationRisk: condensationRisk ?? this.condensationRisk,
    weatherStale: weatherStale ?? this.weatherStale,
    pumpsState: pumpsState ?? this.pumpsState,
    dehumidifierState: dehumidifierState ?? this.dehumidifierState,
    quietStartHour: quietStartHour ?? this.quietStartHour,
    quietEndHour: quietEndHour ?? this.quietEndHour,
  );
}
