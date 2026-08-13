# Radiant Cooling System

[![GitHub repo](https://img.shields.io/badge/repo-qppd%2Fradiant--cooling-181717?logo=github&style=flat)](https://github.com/qppd/radiant-cooling)
[![ESP32](https://img.shields.io/badge/ESP32-WROOM--32-8B9DC3?style=flat)](#hardware)
[![Arduino IDE](https://img.shields.io/badge/Arduino%20IDE-2.x-00979D?logo=arduino&style=flat)](#firmware)
[![Flutter](https://img.shields.io/badge/Flutter-Android-02569B?logo=flutter&style=flat)](#companion-app)
[![Firebase](https://img.shields.io/badge/Firebase-Realtime%20Database-FFCA28?logo=firebase&style=flat)](#firebase-setup)
[![ESP-NOW](https://img.shields.io/badge/ESP--NOW-mesh-00A9E0?style=flat)](#system-architecture)

> Intelligent radiant cooling for the home — three ESP32 boards that talk to
> each other over **ESP-NOW**, sync to **Firebase Realtime Database**, and are
> monitored from an **Android (Flutter)** app.

---

## Table of Contents

- [About](#about)
- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware](#hardware)
- [Repository Structure](#repository-structure)
- [Getting Started](#getting-started)
  - [Firmware](#firmware)
  - [Companion App](#companion-app)
- [Firebase Setup](#firebase-setup)
- [Documentation](#documentation)
- [Tech Stack](#tech-stack)
- [Roadmap](#roadmap)
- [License](#license)
- [Author](#author)

---

## About

A complete control and monitoring system for a **radiant cooling**
installation. Three ESP32 boards form a wireless mesh:

- **RadiantCoolingMonitor** (gateway) reads **6× DS18B20** water/pipe
  temperatures, collects data from the other boards, receives outdoor
  weather from the Flutter app, and pushes everything to Firebase — while
  also computing the chiller pump control (weather-driven + condensation
  protection).
- **WaterChillerController** switches **2 water pumps (SSR)** on command from
  the gateway.
- **DehumidifierController** holds indoor humidity at **55% RH** with its
  DHT22.

No MQTT broker, no cloud middleware — the boards use **ESP-NOW** directly,
and the gateway is the only device with Wi-Fi.

## Features

- **ESP-NOW mesh** — peer-to-peer, no router or broker required between boards
- **Two-way Firebase** — realtime *streaming* of config changes from the app,
  plus telemetry/state writes (Mobizt `FirebaseClient`, async, non-blocking)
- **Weather-aware pump control** — combines WeatherAPI.com dew point,
  indoor humidity, water/pipe temperatures and indoor air (DHT22);
  anti-condensation protection on the coldest pipe
- **Dehumidifier** — holds indoor RH at 55% with hysteresis
- **WiFiManager provisioning** — captive portal on first boot; hold the
  reset button 3 s to re-provision
- **Flutter Android app** — monitoring and configuration (scaffold in place)
- **Modular firmware** — every component and library encapsulated in its own
  class (sensor, SSR, ESP-NOW, JSON, Firebase, Weather, Climate)

## System Architecture

```
┌────────────────┐   ESP-NOW   ┌──────────────────────────┐
│WaterChiller    │◄───────────►│RadiantCoolingMonitor     │
│Controller      │             │(gateway: WiFiManager +   │
└────────────────┘             │ Firebase stream +        │
┌────────────────┐   ESP-NOW   │ Firebase + ESP-NOW)      │
│Dehumidifier    │◄───────────►│                          │
│Controller      │             └────────────┬─────────────┘
└────────────────┘                          │ HTTPS
                                            ▼
                             ┌────────────────────────────┐
                             │ Firebase Realtime Database  │
                             └────────────┬───────────────┘
                                          │ Firebase SDK
                                          ▼
                             ┌────────────────────────────┐
                             │ RadiantCooling App          │
                             │ (Flutter, Android)          │
                             └────────────────────────────┘
```

Full diagrams: [`docs/diagrams/`](docs/diagrams/) · [`docs/schematic/pin-map.md`](docs/schematic/pin-map.md)

## Hardware

| Board                       | Sensors                           | Actuators (SSR)           | Role     |
| --------------------------- | --------------------------------- | ------------------------- | -------- |
| `RadiantCoolingMonitor`     | 6× DS18B20 (1-Wire, GPIO 18)      | —                         | Gateway  |
| `WaterChillerController`    | 1× DS18B20 (GPIO 22)              | 2× SSR → pumps (19, 21)   | Peer     |
| `DehumidifierController`    | 1× DHT22 (GPIO 32)                | 1× SSR → dehumidifier (23)| Peer     |

All pins are boot/Wi-Fi-safe for the 38-pin ESP32 variant — see the
[pin map](docs/schematic/pin-map.md).

Each board is powered by its **own** AC-DC supply (**220 V AC → 5 V DC,
3 A**, feeding the `5V`/`VIN` pin) — no shared rail between boards, see
[`references/power-supply.md`](references/power-supply.md).

## Repository Structure

```
radiant-cooling/
├── docs/
│   ├── api.md                 # ESP-NOW + Firebase protocol
│   ├── tech-stacks.md         # technology choices
│   ├── diagrams/              # architecture, block & flow diagrams
│   └── schematic/             # pin map & electrical references
├── references/                # reference material (datasheets, notes)
└── src/
    ├── app/
    │   └── RadiantCooling/    # Flutter companion app (Android)
    └── esp/
        ├── RadiantCoolingMonitor/     # gateway firmware
        ├── WaterChillerController/    # pump controller firmware
        ├── DehumidifierController/    # dehumidifier firmware
        └── README.md                  # firmware module documentation
```

## Getting Started

### Prerequisites

- [Arduino IDE 2.x](https://www.arduino.cc/en/software) with the
  **esp32 by Espressif** board package
- Arduino libraries: `DallasTemperature`, `OneWire`, `DHT sensor library`,
  `ArduinoJson`, `FirebaseClient` (Mobizt), `WiFiManager` (tzapu)
- [Flutter SDK](https://flutter.dev) (for the app)
- A [Firebase](https://firebase.google.com) project with Realtime Database
- A free [WeatherAPI.com](https://www.weatherapi.com) API key (used by the
  Flutter app — the firmware never stores it)

### Firmware

1. Open each sketch folder in `src/esp/` with Arduino IDE, select the correct
   ESP32 board + COM port, and upload.
2. Fill in the config headers: on the gateway copy
   `FIREBASE_CONFIG.example.h` → `FIREBASE_CONFIG.h` (git-ignored), and
   set the peer/gateway MAC addresses and the `SYSTEM_ID` in `Config.h`
   for each board. The WeatherAPI key lives in the Flutter app only.
3. On first boot the gateway opens a **WiFiManager portal** (AP
   `RadiantCooling-AP`) — connect to it from your phone and enter your
   network credentials.
4. Fix the router to a fixed Wi-Fi channel (1, 6, or 11) so ESP-NOW works.

Details: [`src/esp/README.md`](src/esp/README.md)

### Companion App

```bash
cd src/app/RadiantCooling
flutter pub get
flutter run
```

Setup before first run:

1. Use the same Firebase project as the gateway; add the Android app
   (package `com.radiantcooling.radiant_cooling`), download
   `google-services.json` into `android/app/`, and wire the
   `google-services` Gradle plugin — see
   [`references/flutter-android-firebase.md`](references/flutter-android-firebase.md).
2. Copy `lib/config/app_config.example.dart` → `lib/config/app_config.dart`
   and fill in the **WeatherAPI.com key** (git-ignored — the firmware never
   stores it). The app polls WeatherAPI.com and writes the outdoor
   conditions to `radiant/config/weather`, which the gateway streams.
3. On first launch, **link** the app to your system by entering the
   `SYSTEM_ID` from the gateway's `Config.h` (the gateway publishes it in
   `radiant/devices/<SYSTEM_ID>`).

The app currently provides **system linking** and **outdoor weather
publishing**; the dashboard and control screens are next on the roadmap.

## Firebase Setup

1. Create a Firebase project and enable **Realtime Database**.
2. Enable **Email/Password** authentication in *Auth → Sign-in method* for
   the **gateway** (create a dedicated account, e.g.
   `gateway@radiant-cooling.local`). Enable **Anonymous** too if the app
   uses anonymous sign-in.
3. Copy `FIREBASE_CONFIG.example.h` → `FIREBASE_CONFIG.h` in the gateway
   folder and paste the database URL, **Web API key**, and (if using
   email/password auth) credentials — the file is git-ignored so secrets
   stay local.
4. Apply the security rules in
   [`docs/firebase-security-rules.json`](docs/firebase-security-rules.json)
   (paste into *Realtime Database → Rules* or deploy with
   `firebase deploy --only database`). The gateway must use a dedicated
   **email/password** account (replace the placeholder email in the rules) —
   see [`docs/api.md`](docs/api.md#8-firebase-security-rules--auth).

## Documentation

| Document                                              | Contents                                   |
| ----------------------------------------------------- | ------------------------------------------ |
| [`docs/api.md`](docs/api.md)                          | ESP-NOW protocol + Firebase data schema    |
| [`docs/tech-stacks.md`](docs/tech-stacks.md)          | Technology decisions                       |
| [`docs/diagrams/system-architecture.md`](docs/diagrams/system-architecture.md) | System layers            |
| [`docs/diagrams/block-diagram.md`](docs/diagrams/block-diagram.md)             | Hardware blocks         |
| [`docs/diagrams/flow-chart.md`](docs/diagrams/flow-chart.md)                   | Control logic           |
| [`docs/schematic/pin-map.md`](docs/schematic/pin-map.md)                       | Safe pin assignments    |
| [`src/esp/README.md`](src/esp/README.md)              | Firmware module layout   |
| [`references/`](references/)                          | Per-topic references (ESP-NOW, Firebase, sensors, pins…) |

## Tech Stack

| Layer           | Technology                                        |
| --------------- | ------------------------------------------------- |
| Firmware        | ESP32 (WROOM-32), C/C++, Arduino IDE              |
| Mesh            | ESP-NOW (built into the ESP32 core)               |
| Cloud           | Firebase Realtime Database + FirebaseClient       |
| Weather         | WeatherAPI.com (key in the Flutter app)           |
| WiFi provisioning | WiFiManager (captive portal)                    |
| App             | Flutter (Android)                                 |

## Roadmap

- [x] ESP-NOW handlers + telemetry (receive queue, cmd/config processing, heartbeat) on all three boards
- [x] Wire the `onConfigStream` handler (control params + peer forwarding)
- [ ] Flutter app: Firebase SDK, dashboard, control screens
- [x] Firebase security rules file in `docs/` (`firebase-security-rules.json`)
- [x] Unit tests for `ClimateControl` (dew point + pump decision, `src/esp/tests/`)
- [ ] `LICENSE` file

## License

This project is provided under the **MIT License** — a `LICENSE` file will be
added soon.

## Author

<p align="left">
  <img src="https://avatars.githubusercontent.com/u/140778693?v=4" width="90" height="90" alt="avatar" align="left" style="border-radius:50%" />
  <b>Sajed Lopez Mendoza</b><br/>
  <i>Building intelligent solutions</i><br/>
  136 Sitio Crossing, Ilaya Panaon, Unisan, Quezon 4305<br/>
  <a href="https://github.com/qppd">GitHub: @qppd</a> ·
  <a href="https://www.linkedin.com/in/sajed-mendoza">LinkedIn</a>
</p>

<br/>

Project by [**qppd**](https://github.com/qppd) · Built with ESP32, Flutter & Firebase
