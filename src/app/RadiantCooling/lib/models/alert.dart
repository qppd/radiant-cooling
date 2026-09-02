/// Alert event model for the local event log.
///
/// Alerts are detected from Firebase stream changes (gateway offline,
/// sensor failure, condensation risk, etc.) and stored locally for
/// the Alerts screen.
library;

/// Severity level of an alert.
enum AlertSeverity { critical, warning, info }

/// Type of alert event detected from the system.
enum AlertType {
  gatewayOffline,
  sensorFailure,
  condensationRisk,
  weatherStale,
  dehumidifierOff,
  pumpsOff,
  systemLinked,
}

/// A single alert event in the log.
class Alert {
  const Alert({
    required this.type,
    required this.severity,
    required this.title,
    required this.message,
    required this.timestamp,
  });

  final AlertType type;
  final AlertSeverity severity;
  final String title;
  final String message;
  final DateTime timestamp;

  /// Round-trip through JSON for local storage.
  Map<String, dynamic> toJson() => {
    'type': type.name,
    'severity': severity.name,
    'title': title,
    'message': message,
    'timestamp': timestamp.millisecondsSinceEpoch,
  };

  factory Alert.fromJson(Map<String, dynamic> json) => Alert(
    type: AlertType.values.firstWhere((e) => e.name == json['type']),
    severity: AlertSeverity.values.firstWhere(
      (e) => e.name == json['severity'],
    ),
    title: json['title'] as String,
    message: json['message'] as String,
    timestamp: DateTime.fromMillisecondsSinceEpoch(json['timestamp'] as int),
  );
}
