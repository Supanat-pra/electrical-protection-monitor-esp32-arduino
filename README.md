# Remote Electrical Protection Monitoring System (IoT Prototype)

> An IoT prototype simulating an industrial electrical protection relay system — built on ESP32 with real-time MQTT telemetry, Node-RED data pipeline, InfluxDB time-series storage, and Grafana live dashboard.

![Platform](https://img.shields.io/badge/Platform-ESP32%20NodeMCU--32S-blue)
![Protocol](https://img.shields.io/badge/Protocol-MQTT%20%2F%20TLS-green)
![Database](https://img.shields.io/badge/Database-InfluxDB-orange)
![Dashboard](https://img.shields.io/badge/Dashboard-Grafana-red)
![Deployment](https://img.shields.io/badge/Deployment-Docker-lightblue)
![Language](https://img.shields.io/badge/Firmware-C%2B%2B%20Arduino-yellow)

---

## 📋 Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [Protection Logic](#protection-logic)
- [Hardware](#hardware)
- [Tech Stack](#tech-stack)
- [MQTT Topic Structure](#mqtt-topic-structure)
- [Data Schema](#data-schema)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Flux Queries](#flux-queries)
- [Dashboard](#dashboard)
- [Future Improvements](#future-improvements)

---

## Overview

This project simulates a real-world **electrical protection relay system** using edge computing on an ESP32 microcontroller. It reflects concepts used in industrial electrical protection — including overcurrent protection, over-temperature tripping, and fail-safe relay control — implemented as an IoT prototype with full cloud-connected monitoring.

**Key concepts demonstrated:**

- **Edge protection logic** — state machine runs entirely on the ESP32, no cloud dependency for trip decisions
- **De-energize-to-trip philosophy** — relay de-energizes on fault, ensuring load disconnects on power loss (fail-safe)
- **LOCKOUT state** — system latches after a trip event, requiring manual reset before re-energization
- **Bi-directional MQTT** — telemetry published to cloud, reset commands received from cloud
- **Time-series data pipeline** — structured sensor data flows from hardware to dashboard in real time

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        EDGE LAYER                               │
│                                                                 │
│   Potentiometer ──→                                             │
│   (Load Simulation)  │                                          │
│                      ├──→ ESP32 NodeMCU-32S                     │
│   DHT11 ─────────→   │    (Protection Logic)  ──→ Relay Output  │
│   (Temperature)      │    State Machine                         │
│                      │    C++ / Arduino                         │
└──────────────────────┼──────────────────────────────────────────┘
                       │ MQTT / TLS (Port 8883)
                       ↓
┌─────────────────────────────────────────────────────────────────┐
│                         BROKER LAYER                            │
│                                                                 │
│                         HiveMQ Cloud                            │
│                  (MQTT Broker / TLS / Auth)                     │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ↓
┌─────────────────────────────────────────────────────────────────┐
│                        PIPELINE LAYER                           │
│                                                                 │
│                            Node-RED                             │
│           MQTT Subscribe → Parse JSON → Format → Write          │
│                                                                 │
│                               ↓                                 │
│                            InfluxDB                             │
│                     (Time-Series Database)                      │
│                  Bucket: electrical_protection                  │
└──────────────────────┬──────────────────────────────────────────┘
                       │
                       ↓
┌─────────────────────────────────────────────────────────────────┐
│                      VISUALIZATION LAYER                        │
│                                                                 │
│                           Grafana                               │
│           Live Gauges │ Trend Graphs │ State Indicator          │
│                       Auto-refresh: 5s                          │
└─────────────────────────────────────────────────────────────────┘
```

---

## Protection Logic

The system implements a **4-state protection state machine** running entirely on the ESP32:

```
              load > 85%
  NORMAL ─────────────────→ WARNING
    ↑                           │
    │       load ≤ 80%          │  load > 95%
    └───────────────────────────┘  temp > 45°C
                                        │
                                        ↓
                                     TRIPPED
                                        │
                                   (auto latch)
                                        ↓
                                     LOCKOUT ←── stays here
                                        │         until reset
                               ┌────────┴────────┐
                               │                 │
                          Auto-reset          MQTT RESET
                      (load < 30% AND       command received
                        temp < 35°C)
                               │                 │
                               └────────┬────────┘
                                        ↓
                                     NORMAL
```

### Trip Conditions

| Condition        | Threshold   | State   |
| ---------------- | ----------- | ------- |
| High load        | load > 85%  | WARNING |
| Overcurrent      | load > 95%  | TRIPPED |
| Over-temperature | temp > 45°C | TRIPPED |
| Post-trip latch  | —           | LOCKOUT |

### Relay Behavior (De-energize to Trip)

| System State      | Relay Coil   | NO Contact | Load                |
| ----------------- | ------------ | ---------- | ------------------- |
| NORMAL            | Energized    | Closed     | Connected (Running) |
| WARNING           | Energized    | Closed     | Connected (Running) |
| TRIPPED / LOCKOUT | De-energized | Open       | Disconnected (Safe) |

> **Design Note:** The relay uses **Energize-to-Normal** with NO contact — load is connected only when system is healthy and relay is actively energized. On power loss or ESP32 failure, relay de-energizes and load disconnects automatically.

---

## Hardware

### Components

| Component                      | Purpose            | Pin          |
| ------------------------------ | ------------------ | ------------ |
| ESP32 NodeMCU-32S              | Main controller    | —            |
| Potentiometer 10kΩ             | Simulate load %    | GPIO34 (ADC) |
| DHT11                          | Temperature sensor | GPIO4        |
| Relay Module (5V, Active High) | Protection output  | GPIO26       |

### Wiring Diagram

```
ESP32          Potentiometer
3.3V    ───→  VCC (left pin)
GND     ───→  GND (right pin)
GPIO34  ───→  WIPER (middle pin)

ESP32          DHT11
3.3V    ───→  VCC
GND     ───→  GND
GPIO4   ───→  DATA

ESP32          Relay Module
VIN(5V) ───→  VCC
GND     ───→  GND
GPIO26  ───→  IN
         ↑
    Active High
    NO terminal used
```

---

## Tech Stack

| Layer           | Technology                          |
| --------------- | ----------------------------------- |
| Firmware        | C++ (Arduino framework), PlatformIO |
| Microcontroller | ESP32 NodeMCU-32S                   |
| MQTT Broker     | HiveMQ Cloud (TLS, port 8883)       |
| Data Pipeline   | Node-RED                            |
| Time-Series DB  | InfluxDB 2.7                        |
| Dashboard       | Grafana                             |
| Deployment      | Docker                              |

---

## MQTT Topic Structure

| Topic                                              | Direction     | Purpose                  |
| -------------------------------------------------- | ------------- | ------------------------ |
| `iot/supanat-p/esp32-electrical-protect/telemetry` | ESP32 → Cloud | Sensor data every 2s     |
| `iot/supanat-p/esp32-electrical-protect/alert`     | ESP32 → Cloud | State change events only |
| `iot/supanat-p/esp32-electrical-protect/command`   | Cloud → ESP32 | Remote reset command     |

### Telemetry Payload

```json
{
  "ts": 42,
  "load_pct": 86.0,
  "temp_c": 28.0,
  "state": "WARNING",
  "relay": "NORMAL(ON)"
}
```

### Alert Payload

```json
{
  "ts": 86,
  "alert": true,
  "state": "TRIPPED",
  "previous_state": "WARNING"
}
```

### Reset Command

```
Topic:   iot/supanat-p/esp32-electrical-protect/command
Payload: RESET
```

---

## Data Schema

### InfluxDB Measurements

**Measurement: `telemetry`**

| Field      | Type    | Description                           |
| ---------- | ------- | ------------------------------------- |
| `load_pct` | float   | Load percentage 0–100%                |
| `temp_c`   | float   | Temperature in °C                     |
| `state`    | string  | NORMAL / WARNING / TRIPPED / LOCKOUT  |
| `relay`    | integer | 1 = ON (connected), 0 = OFF (tripped) |

**Tags:**

| Tag      | Value                  |
| -------- | ---------------------- |
| `device` | electrical-protection1 |

**Measurement: `alert`**

| Field            | Type    | Description                |
| ---------------- | ------- | -------------------------- |
| `alert`          | boolean | Always true                |
| `state`          | string  | New state after transition |
| `previous_state` | string  | State before transition    |

---

## Project Structure

```
esp32-electrical-protection/
├── src/
│   ├── main.cpp              # ESP32 C++ firmware
├── platformio.ini            # PlatformIO config
├── include/
│   ├── credentials.h         # WiFi & MQTT credentials (gitignored)
├── node-red/
│   └── flows.json            # Node-RED flow export
├── queries/
│   └── queries.flux          # InfluxDB Flux queries
│   └── grafana.flux          # Grafana Flux queries
├── docker/
│   └── docker-compose.yml    # InfluxDB + Grafana stack
├── docs/
│   └── architecture.png      # System architecture diagram
└── README.md
```

---

## Getting Started

### Prerequisites

- PlatformIO (VS Code extension)
- Docker Desktop
- Node-RED (`npm install -g node-red`)
- HiveMQ Cloud account (free tier)

### 1 — Firmware Setup

Clone the repo and create `include/credentials.h`:

```cpp
#pragma once
const char* wifi_ssid     = "YOUR_WIFI_SSID";
const char* wifi_password = "YOUR_WIFI_PASSWORD";
const char* mqtt_user     = "YOUR_HIVEMQ_USER";
const char* mqtt_password = "YOUR_HIVEMQ_PASSWORD";
```

Open project in PlatformIO, build and upload to ESP32.

### 2 — Start InfluxDB + Grafana

```bash
cd docker
docker-compose up -d
```

Open InfluxDB at `http://localhost:8086` and create:

```
Organization: electrical-protection
Bucket:       electrical_protection
Retention:    30 days
```

Generate an API token and save it.

### 3 — Start Node-RED

```bash
node-red
```

Open `http://localhost:1880`, import `node-red/flows.json`, update MQTT broker credentials and InfluxDB token, then Deploy.

### 4 — Open Grafana Dashboard

Open `http://localhost:3000`, connect InfluxDB data source, import dashboard, and set auto-refresh to 5s.

---

## Flux Queries

### Average load per minute

```flux
from(bucket: "electrical_protection")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "load_pct")
  |> aggregateWindow(every: 1m, fn: mean, createEmpty: false)
```

### Max temperature

```flux
from(bucket: "electrical_protection")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "temp_c")
  |> max()
```

### Count trip events

```flux
from(bucket: "electrical_protection")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "alert")
  |> filter(fn: (r) => r._field == "state")
  |> filter(fn: (r) => r._value == "TRIPPED")
  |> count()
```

### Load vs relay correlation

```flux
from(bucket: "electrical_protection")
  |> range(start: -30m)
  |> filter(fn: (r) => r._measurement == "telemetry")
  |> filter(fn: (r) => r._field == "load_pct" or r._field == "relay")
  |> pivot(
      rowKey: ["_time"],
      columnKey: ["_field"],
      valueColumn: "_value"
    )
```

---

## Dashboard

The Grafana dashboard displays:

| Panel                 | Type        | Description                            |
| --------------------- | ----------- | -------------------------------------- |
| Load %                | Gauge       | Live load with WARNING/TRIP thresholds |
| Temperature           | Gauge       | Live temp with TRIP threshold          |
| System State          | Stat        | Color-coded state indicator            |
| Load Over Time        | Time series | 30-min trend with threshold bands      |
| Temperature Over Time | Time series | 30-min temperature trend               |
| Relay Status          | Step graph  | 0/1 relay state over time              |

---

## Future Improvements

- **NTP real datetime** — replace `millis()` uptime with ISO 8601 timestamp
- **TypeScript + React dashboard** — custom web dashboard replacing Grafana for full-stack showcase
- **OPC-UA protocol** — industry standard for SCADA integration
- **Hardware watchdog** — external circuit ensuring relay trips on ESP32 failure
- **Multiple devices** — extend to pump2, pump3 with device tag filtering
- **TLS certificate validation** — replace `setInsecure()` with proper CA certificate
- **Retention policy** — automated 30-day data cleanup in InfluxDB
- **Docker Compose** — add Node-RED to containerized stack

---

## Author

**Supanat Prakobkham**
Electrical Engineer & Software Developer

[![LinkedIn](https://img.shields.io/badge/LinkedIn-supanat--prakobkham-blue)](https://www.linkedin.com/in/supanat-prakobkham)
[![GitHub](https://img.shields.io/badge/GitHub-Supanat--pra-black)](https://github.com/Supanat-pra)
