# MASTERWIFI12T

**A full-stack IoT smart home controller built from scratch on the ESP8266 platform** -- bridging embedded firmware, real-time communication protocols, cloud services, and a local web interface into a single, cohesive system.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![Platform](https://img.shields.io/badge/Platform-ESP8266-orange)
![Version](https://img.shields.io/badge/Version-8.1-green)
![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue)

---

## Overview

MASTERWIFI12T is the central brain of a custom-built smart home automation system. It acts as a **master controller** that manages relays, fans, and sensors -- coordinating between physical hardware, a serial-connected slave device, an MQTT cloud backend, and voice assistants like Alexa and Google Home via Sinric.

This project was designed, architected, and implemented end-to-end as a real product under [The Next Move (TNM)](http://www.thenextmove.in).

---

## Architecture

```
                         +---------------------+
                         |   Alexa / Google     |
                         |   Home (Voice)       |
                         +--------+------------+
                                  |  WebSocket
                                  v
+-------------+    MQTT     +-----+-----------+    Serial/JSON    +-----------+
|  Mobile /   | <---------> |  MASTERWIFI12T  | <---------------> |   Slave   |
|  Dashboard  |             |   (ESP8266)     |                   |  Device   |
+-------------+             +--+-----------+--+                   +-----------+
                               |           |
                          I2C  |           |  I2C
                               v           v
                          +--------+  +--------+
                          | DS3231 |  | EEPROM |
                          |  RTC   |  | 32KB   |
                          +--------+  +--------+
```

---

## Key Features

| Feature | Description |
|---|---|
| **Multi-Device Control** | Manages up to 5 relays, 5 variable-speed fans (4 speeds), and 5 sensors simultaneously |
| **Dual Cloud Integration** | MQTT broker for dashboard control + Sinric WebSocket for voice assistant support |
| **Master-Slave Communication** | Custom JSON-over-UART protocol with handshake, validation, and bidirectional state sync |
| **Scheduling Engine** | 4 independent timers with per-day-of-week scheduling and separate ON/OFF trigger times |
| **Scene Automation** | 4 configurable scenes that activate predefined relay/fan state combinations |
| **Captive Portal WiFi Setup** | Embedded web server with network scanning, signal strength visualization, and one-tap provisioning |
| **OTA Firmware Updates** | Remote firmware deployment via HTTP from a cloud endpoint |
| **Persistent Storage** | Triple-layer storage: internal EEPROM, external I2C EEPROM (AT24C256), and LittleFS filesystem |
| **Real-Time Clock** | DS3231 RTC with NTP internet sync for accurate offline timekeeping |
| **Async Task Scheduler** | Non-blocking cooperative multitasking loop managing all subsystems concurrently |

---

## Technical Deep Dive

### Communication Protocols Implemented

**MQTT (Message Queuing Telemetry Transport)**
- Full publish/subscribe implementation with topic-based routing
- Hierarchical topic structure: `{deviceId}/relay/{n}/value/`, `{deviceId}/system/timer/{n}/ontimer/hour/`, etc.
- Retained messages for state persistence across reconnections
- Structured handshake that syncs all device states, timers, scenes, and clock data on connect

**JSON-over-UART (Custom Master-Slave Protocol)**
- Designed a custom serial protocol with handshake negotiation and role identification
- Character-by-character JSON parsing with brace-counting for packet boundary detection
- Dynamic device array allocation based on slave-reported capabilities
- Bidirectional state synchronization with change-flag tracking

**Sinric WebSocket**
- Persistent WebSocket connection to `iot.sinric.com` for voice control
- Custom heartbeat mechanism (5-minute keepalive) to maintain connection
- JSON command parsing for `setPowerState` actions mapped to relay control

**mDNS + HTTP**
- Local network discovery via `{deviceId}.local` hostname
- Embedded HTTP server serving a WiFi configuration portal with responsive HTML/CSS

### State Management & Change Detection

Every device (relay, fan, sensor) maintains:
- Current state and value
- Per-source change flags (`lastmqttcommand`, `lastslavecommand`)
- Aggregate change counters for efficient batch processing

This architecture ensures that a state change from MQTT is forwarded to the slave (and vice versa) without creating feedback loops -- a common pitfall in bidirectional IoT systems.

### Persistent Storage Architecture

```
Internal EEPROM (512B)     -->  WiFi credentials (fast boot)
External I2C EEPROM (32KB) -->  Timers & scenes (structured binary data)
LittleFS Filesystem        -->  Sinric API keys & device IDs (JSON files)
```

Each storage layer is chosen based on access pattern: EEPROM for boot-critical data, external EEPROM for structured automation configs, and filesystem for user-facing credentials.

### Async Task Scheduler

The firmware runs a cooperative multitasking loop -- no RTOS, no threads. Each subsystem handler is called every cycle and manages its own timing:

```
loop() -> asynctasks()
           |-- timesensor_handler()   // RTC + NTP sync
           |-- timer_handler()        // Schedule evaluation
           |-- scene_handler()        // Scene triggering
           |-- wifi_handler()         // Connection management
           |-- device_handler()       // Change detection
           |-- mdns_handler()         // Web server
```

This pattern keeps the system responsive while running on a single-threaded microcontroller with ~80KB of RAM.

---

## Tech Stack

| Layer | Technologies |
|---|---|
| **Microcontroller** | ESP8266 (ESP12E) -- 80MHz, 80KB RAM, integrated WiFi |
| **Language** | C++17 |
| **Networking** | ESP8266WiFi (AP+STA dual mode), PubSubClient (MQTT), WebSocketsClient |
| **Cloud** | MQTT broker, Sinric (Alexa/Google Home), Firebase Cloud Functions (OTA) |
| **Data Formats** | ArduinoJson for serialization/deserialization across all protocols |
| **Storage** | LittleFS, I2C EEPROM (AT24C256), Internal EEPROM |
| **Timekeeping** | DS3231 RTC module + NTP client with automatic sync |
| **Web** | Embedded HTML/CSS served from firmware, mDNS for discovery |
| **Hardware Interfaces** | I2C (Wire), UART (Serial), GPIO (interrupts, digital I/O) |

---

## Hardware Schematic

| Component | Interface | Address/Pin |
|---|---|---|
| ESP8266 (ESP12E) | -- | Main MCU |
| DS3231 RTC | I2C | Default (0x68) |
| AT24C256 EEPROM | I2C | 0x57 |
| Status LED | GPIO | Pin 2 |
| Input Button | GPIO (interrupt) | Pin 12 |
| Slave Device | UART | 9600 baud |
| Debug Console | UART | 115200 baud |

---

## MQTT Command Reference

```
# Relay Control
{id}ESP/relay/1/value/     -->  "1" (ON) / "0" (OFF)
{id}ESP/relay/all/value/   -->  "1" (all ON) / "0" (all OFF)

# Fan Control
{id}ESP/fan/1/state/       -->  "1" (ON) / "0" (OFF)
{id}ESP/fan/1/value/       -->  "1"-"4" (speed level)

# Timer Configuration
{id}ESP/system/timer/1/ontimer/hour/    -->  "14" (2 PM)
{id}ESP/system/timer/1/ontimer/minute/  -->  "30"
{id}ESP/system/timer/1/ontimer/week/    -->  "1111100" (Mon-Fri)

# Scene Control
{id}ESP/system/scene/1/trigger/         -->  "1" (activate)

# System
{id}ESP/system/update/                  -->  "" (trigger OTA update)
{id}ESP/system/clock/                   -->  "" (request time sync)
```

---

## Project Structure

```
MASTERWIFI12T/
|-- Firmware/
|   |-- Firmware.ino          # Entry point, initialization, main loop
|   |-- HomeHub.h             # Data structures, class declaration (373 lines)
|   |-- HomeHub.cpp           # Core implementation (2,073 lines)
|   |-- eepromi2c.h           # Templated I2C EEPROM read/write utilities
|   +-- data/
|       +-- wifi.txt           # Persisted WiFi credentials (LittleFS)
|
|-- Modules/
|   |-- Wifi+ Support Library/       # Standalone WiFi reference implementation
|   +-- Storage+ Support Library/    # DS3231 + I2C storage examples
|
|-- MQTT Scene Commands.rtf    # Scene command documentation
|-- MQTT Timer Commands.rtf    # Timer command documentation
|-- LICENSE                    # GNU GPL v3
+-- README.md
```

---

## Skills Demonstrated

- **Embedded Systems Engineering** -- firmware development on resource-constrained hardware (80KB RAM, single-threaded)
- **IoT Protocol Design** -- implemented MQTT, WebSocket, and a custom JSON serial protocol from scratch
- **Full-Stack Thinking** -- hardware interfacing (I2C, UART, GPIO), firmware logic, cloud integration, and web UI in one codebase
- **Systems Architecture** -- designed a modular handler-based architecture with cooperative multitasking
- **State Machine Design** -- bidirectional state sync across three interfaces (MQTT, serial, WebSocket) without feedback loops
- **Data Persistence** -- multi-tier storage strategy optimized for access patterns and hardware constraints
- **Network Programming** -- WiFi management (AP+STA), mDNS discovery, HTTP server, MQTT pub/sub, WebSocket keepalive
- **C++ on Embedded** -- lambda callbacks, templates (EEPROM read/write), `std::vector`, `std::function` on a microcontroller
- **Product Engineering** -- OTA updates, error handling, debug macros, captive portal for end-user WiFi setup

---

## Version History

| Version | Highlights |
|---|---|
| **v8.1** | Latest stable -- full timer/scene persistence, refined MQTT handshake |
| **v8.0** | Major architecture overhaul |
| **v1.0 - v7.x** | Iterative development from basic relay control to full smart home platform |

---

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).

---

<p align="center"><i>Built with care by Vikhyat Chauhan</i></p>
