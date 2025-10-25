<p align="center">
  <img width="252" height="80" alt="dhumketux-logo-picv89pfmqg232dzle0szl8ygpq038fuxufww8t3pc" src="https://github.com/user-attachments/assets/d60008d8-051f-476f-9584-1da0d1d83e0f" />

</p>

<h1 align="center">🚀 DhumketuX LPU – Bluepill F103C8T6 Firmware</h1>

<p align="center">
  High-efficiency, non-blocking firmware for the DhumketuX Launch Pad Unit (LPU) — built for precise telemetry and ignition control using the STM32 Bluepill.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Microcontroller-STM32F103C8T6-blue?logo=stmicroelectronics" />
  <img src="https://img.shields.io/badge/Wireless-LoRa%20RA--02-green?logo=radio" />
  <img src="https://img.shields.io/badge/UI-Web%20Serial%20Dashboard-orange" />
  <img src="https://img.shields.io/badge/License-MIT-lightgrey" />
</p>

---

## 🧭 Overview

The **DhumketuX LPU (Launch Pad Unit)** is the on-field controller responsible for:
- Real-time thrust measurement via HX711 load cell interface.
- Telemetry broadcasting through LoRa RA-02.
- Safe ignition and servo-based arming control.
- Non-blocking command execution (no `delay()` except ignition pulse).
- Optimized for **Bluepill STM32F103C8T6** hardware.

### 🔑 Key Highlights
- **Non-blocking architecture:** All timing handled via `millis()`.
- **Fail-safe LoRa initialization:** System halts and flashes LED if initialization fails.
- **Strict C-style programming:** No `String` class, no dynamic memory.
- **Optimized for performance:** Compiled with `-O2` for efficiency.

---

## Board Profile: DhumketuX_LPU

| Parameter | Value | Rationale |
|------------|--------|-----------|
| **Board Name** | `DhumketuX_LPU` | Unique identifier for this firmware. |
| **MCU** | STM32F103C8T6 | Standard Bluepill MCU. |
| **Upload Method** | STLink or Serial | Compatible with STM32CubeProgrammer. |
| **Bootloader Type** | `generic_boot20` | Common Bluepill bootloader. |
| **CPU Speed** | 72 MHz | Maximum stable frequency. |
| **Flash Size** | 64 K B (128 K B for extended builds) | Ensures correct memory mapping. |
| **Optimization Level** | `-O2` | Prioritize runtime efficiency. |
| **USB Support** | None / Disabled | Minimize core overhead. |
| **C++ Standard** | C++11 | Balanced compatibility and speed. |
| **Serial Interface** | `Serial` (PA9/PA10) | Primary UART for debugging. |

---

## ⚙️ 1. Architecture & Core Constraints

| Constraint | Detail | Criticality |
|-------------|---------|-------------|
| **MCU** | STM32F103C8T6 (Bluepill) | 🔴 High |
| **Core** | STM32Duino / Arduino_Core_STM32 | 🔴 High |
| **Timing** | Strictly non-blocking (`millis()`-based) | 🔴 Extreme |
| **Memory Policy** | Fixed arrays, no heap allocation | 🟠 Critical |
| **Communication** | Raw binary telemetry struct | 🔵 High |
| **Safety** | LED/Buzzer alert if LoRa fails | 🔴 Extreme |

---

##  2. Hardware Interface & Pinout Reference

### 📡 LoRa SPI Bus (RA-02)

| Function | STM32 Pin | Notes |
|-----------|------------|-------|
| NSS / CS | PA4 | Chip Select |
| RST | PB12 | Hardware reset line |
| DIO0 | PB13 | LoRa IRQ interrupt |
| SCK | PA5 | SPI Clock |
| MISO | PA6 | SPI MISO |
| MOSI | PA7 | SPI MOSI |

###  Sensors & Actuators

| Component | Function | STM32 Pin | Notes |
|------------|-----------|------------|-------|
| HX711 | Thrust Load Cell | DOUT → PB0, SCK → PB1 | 24-bit ADC interface |
| DHT22 | Temperature/Humidity | PA8 | 1-Wire data line |
| Safety Servo | Arm/Disarm | PA9 | PWM output |
| Ignition Relay | Launch Trigger | PC13 | Active-LOW output (300 ms pulse) |
| Status Buzzer/LED | Alert | PB10 | Non-blocking flashing |
| LoRa | SPI Communication | See above | Telemetry link |

---

## 🛰️ 3. Telemetry Data Structure

The firmware transmits telemetry as a **binary struct** for bandwidth efficiency.

```cpp
struct __attribute__((packed)) Telemetry_t {
    float thrust;        // Load cell thrust (N)
    float temperature;   // DHT22 temperature (°C)
    float humidity;      // DHT22 humidity (%)
    int8_t isArmed;      // System state (0: Disarmed, 1: Armed, 2: Launching)
    uint16_t checksum;   // XOR checksum for data integrity
};
// Total size: 15 bytes
Any field or order modification must also be updated in the Ground Station decoder.



4. Command Protocol (Reception)
Command	Action	State Transition
A	Arm system (unlock safety servo)	DISARMED → ARMED
S	Safe lock (disarm)	ARMED → DISARMED
I	Ignite (300 ms relay pulse)	ARMED → LAUNCHING
T	Transmit test telemetry	—

Non-blocking LoRa parsing ensures no delay in command reception.



5. Build and Development Environment Setup
Tool	Recommended Version
Arduino IDE	2.2.1
STM32Duino Core	2.6.0+
Board	Generic STM32F103C series

Library Dependencies
Library	Author	Version
LoRa	Sandeep Mistry	0.8.0
DHT sensor library	Adafruit	1.4.4
Adafruit Unified Sensor	Adafruit	1.1.7
Servo	Arduino	Built-in

Build Configuration
Install STM32Duino Core via Boards Manager.

Select Board: Generic STM32F103C8 (Bluepill).

Configure options:

Upload method: STLink or Serial

CPU speed: 72 MHz

USB support: None

Optimize: O2 (Speed)

Install the above libraries.

Compile & Upload via STM32CubeProgrammer.


 6. Repository Structure
css
Copy code
DhumketuX_LPU/
├── src/
│   ├── LaunchPad_Tx.ino
│   ├── Telemetry.h
│   └── CommandHandler.cpp
├── docs/
│   └── wiring_diagram.png
├── README.md
└── LICENSE


License
This project is licensed under the MIT License — see the LICENSE file for details.


Made by Lian Mollick Nehal
