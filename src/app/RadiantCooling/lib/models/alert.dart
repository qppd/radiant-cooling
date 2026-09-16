library;

enum AlertSeverity { critical, warning, info }

enum AlertType {
  gatewayOffline,
  sensorFailure,
  condensationRisk,
  weatherStale,
  dehumidifierOff,
  pumpsOff,
  systemLinked,
}

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
