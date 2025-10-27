
<p align="center">
  <img width="252" height="80" alt="dhumketux-logo-picv89pfmqg232dzle0szl8ygpq038fuxufww8t3pc" src="https://github.com/user-attachments/assets/447263ab-24ea-4f26-ab0a-907d59a87e52" />
</p>

<h1 align="center">🚀 DhumketuX Engine Test System (DETS)</h1>

<p align="center">
  <b>Real-time rocket engine thrust test system integrating LoRa telemetry, web serial dashboard, and strict C-style embedded firmware.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Microcontroller-Arduino%20Uno-blue?logo=arduino" />
  <img src="https://img.shields.io/badge/Wireless-LoRa%20RA--02-brightgreen?logo=wifi" />
  <img src="https://img.shields.io/badge/UI-Web%20Serial%20Dashboard-orange?logo=googlechrome" />
  <img src="https://img.shields.io/badge/License-MIT-lightgrey?logo=open-source-initiative" />
</p>

---

## 🌍 Overview

**DhumketuX Engine Test System (DETS)** is a precision-engineered telemetry and control platform designed for **real-time static rocket engine testing**.  
It enables wireless thrust data acquisition and ignition control via **LoRa RA-02** and a **Web Serial Dashboard**, ensuring safe, responsive, and reliable operation.

### ✨ Key Highlights
- Dual-unit system: Launch Pad (LP) + Ground Station (GS)
- LoRa-based wireless telemetry link
- Real-time thrust, temperature, and humidity monitoring
- Web Serial Dashboard visualization
- Fully modular firmware with reproducible builds
- **Strictly C-style programming on the MCU** (no `String` objects or dynamic memory allocation) for maximum reliability

---

## ⚙️ System Architecture & Pinout Specification

The system consists of two primary units:  
🛰️ **Ground Station Unit (Receiver)** and 🔥 **Launch Pad Unit (Sensor & Control)**.

Both are powered by Arduino Uno/Nano and communicate using **LoRa RA-02** modules.

---

### 🛰️ Ground Station Unit (Receiver)

| Component        | Role              | Connection Details (Arduino Uno/Nano) |
|------------------|-------------------|---------------------------------------|
| **LoRa RA-02 Module** | SPI Slave Select (CS) | D10 |
|                  | Reset (RST)        | D9  |
|                  | DIO0 (Interrupt/IRQ) | D2 |
|                  | SPI Clock (SCK)    | D13 |
|                  | SPI Master Out (MOSI) | D11 |
|                  | SPI Master In (MISO) | D12 |
| **Status LED**   | Activity Indicator | D8  |

---

### 🔥 Launch Pad Unit (Sensor & Control)

| Component        | Role                   | Connection Details (Arduino Uno/Nano) |
|------------------|------------------------|---------------------------------------|
| **LoRa Module**  | Control Transceiver    | D10, D9, D2 (Identical to GS) |
| **HX711 Load Cell** | Thrust Measurement   | D2 (SCK), D3 (DOUT) |
| **DHT22 Sensor** | Environmental Data     | D6 (DATA) |
| **Ignition Relay** | Control Circuit       | D13 (Trigger) |
| **Safety Servo** | Physical Arming        | D5 (PWM) |

---

## 🛠️ Setup and Installation (Failproof Guide)

Follow this guide **exactly** to ensure successful and reproducible builds.

### 🧩 IDE and Board Manager Versions

| Tool | Recommended Version |
|------|----------------------|
| **Arduino IDE** | v2.2.1 |
| **Arduino AVR Boards** | v1.8.6 |

---

### 📚 Required Libraries

| Library | Author | Version |
|----------|--------|----------|
| **LoRa** | Sandeep Mistry | v0.8.0 |
| **HX711** | bogde | v1.2.3 |
| **DHT sensor library** | Adafruit | v1.4.4 |
| **Adafruit Unified Sensor** | Adafruit | v1.1.7 |
| **Servo** | Arduino | Built-in |

---

### 🪛 Installation Summary

1. Open **Arduino IDE v2.2.1** → Boards Manager → Install **Arduino AVR Boards v1.8.6**.  
2. Go to **Library Manager** → Install each dependency **with the exact versions** listed above.  
3. Connect both Arduinos (LP & GS) and upload respective `.ino` files from `/LaunchPad/` and `/GroundStation/`.

---

## 📡 Communication Protocol Deep Dive

### 4.1. Telemetry (LP → GS → PC)

Telemetry data is transmitted using a **binary struct** for efficient bandwidth use.

#### C++ Struct Definition
```cpp
typedef struct {
    float thrust_kPa;
    float temperature_C;
    float humidity_perc;
} Telemetry_t;
````

**Serialization:**
The Ground Station receives this struct and converts it to a Web Dashboard-friendly string format:

```
Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z
```

| Parameter   | Unit | Description                                   |
| ----------- | ---- | --------------------------------------------- |
| Thrust      | kPa  | Engine chamber pressure (converted to thrust) |
| Temperature | °C   | Ambient or motor temperature                  |
| Humidity    | %RH  | Relative humidity                             |

---

### 4.2. Control Commands (PC → GS → LP)

| Command | Description      | Launch Pad Action                        |
| ------- | ---------------- | ---------------------------------------- |
| **A**   | Arm System       | Activates safety servo (arming position) |
| **S**   | Safe System      | Returns servo to safe position           |
| **T**   | Trigger Ignition | Relay ON (50 ms pulse)                   |
| **I**   | System Info      | Sends status and sensor summary to GS    |

---

## 📁 Repository Structure

```
DhumketuX_DETS/
├── GroundStation/
│   ├── GS_Receiver.ino
│   └── ...
├── LaunchPad/
│   ├── LP_Controller.ino
│   └── ...
├── WebDashboard/
│   ├── index.html
│   ├── app.js
│   └── style.css
├── docs/
│   ├── schematics/
│   └── wiring_diagrams/
└── LICENSE
```

---

## 🧠 Notes for Developers

* Maintain **strict C-style coding** on microcontrollers for deterministic performance.
* Avoid `String`, `malloc()`, or `new` in firmware code.
* Web Dashboard communicates using standard **Web Serial API** (tested on Chrome 116+).
* LoRa channels and sync words must be consistent between LP and GS.

---

## 🧾 License

This project is licensed under the **MIT License**.
See the [LICENSE](./LICENSE) file for full terms.

---

<p align="center">
  <b>Made by Lian Mollick</b> 🚀
</p>

