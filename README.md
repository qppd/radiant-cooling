# Radiant Cooling System

Embedded control and monitoring system for a radiant cooling installation, consisting of
ESP32-based firmware controllers and a companion application for configuration and
monitoring.

## Project Structure

```
radiant-cooling/
├── docs/                      # Project documentation
│   ├── schematic/             # Electrical & plumbing schematics
│   └── diagrams/              # Architecture, wiring & flow diagrams
├── references/                # Reference material (datasheets, notes, etc.)
└── src/
    ├── app/
    │   └── RadiantCooling/    # Companion application (monitoring / configuration)
    └── esp/
        ├── DehumidifierController/    # ESP firmware – dehumidifier control
        ├── RadiantCoolingMonitor/     # ESP firmware – system monitoring
        └── WaterChillerController/    # ESP firmware – water chiller control
```

## Components

| Component                | Description                                        |
| ------------------------ | -------------------------------------------------- |
| `WaterChillerController` | ESP32 firmware — 1x DS18B20, controls 2 water pumps (SSR) |
| `DehumidifierController` | ESP32 firmware — 1x DHT22, controls dehumidifier (SSR)   |
| `RadiantCoolingMonitor`  | ESP32 gateway — 6x DS18B20, relays ESP-NOW data to Firebase |
| `RadiantCooling` (app)   | Flutter companion app for monitoring/configuration (Android) |

## Getting Started

Coming soon — setup instructions will be added as the firmware and app are developed.

## Documentation

- **Schematics:** [`docs/schematic/`](docs/schematic/)
- **Diagrams:** [`docs/diagrams/`](docs/diagrams/)
