<p align="center">
  <img width="252" height="80" alt="DhumketuX Logo" src="https://github.com/user-attachments/assets/447263ab-24ea-4f26-ab0a-907d59a87e52" />
</p>

<h1 align="center">🚀 DhumketuX Engine Test System (DETS)</h1>

<p align="center">
  <b>Modular Bi-Directional Telemetry & Control Framework for Rocket Engine Static Testing</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Microcontroller-Arduino%20Uno-blue?logo=arduino&style=flat-square">
  <img src="https://img.shields.io/badge/Wireless-LoRa%20E32-green?logo=wifi&style=flat-square">
  <img src="https://img.shields.io/badge/UI-Web%20Serial%20Dashboard-orange?logo=javascript&style=flat-square">
  <img src="https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square">
</p>

---

## 🌟 Overview

The **DhumketuX Engine Test System (DETS)** is a **firmware-driven**, modular telemetry and control framework built for safely monitoring, logging, and executing **static fire tests** of small-scale rocket propulsion units.

It features two wirelessly linked subsystems communicating via **LoRa**:

- 🛰️ **Ground Station (GS):** Browser-based command and visualization dashboard.  
- 🔥 **Launch Pad (LP):** Real-time sensor acquisition and ignition control logic.

**Key highlights**
- Safe, two-step ignition arming system  
- Real-time thrust, temperature & humidity data  
- CSV logging for research and analysis  
- Glass-style responsive web interface  

---

## ⚙️ System Architecture

### 🛰️ Ground Station Unit (Receiver & UI Host)

| Component | Role | Connection Details |
|------------|------|--------------------|
| **Arduino Uno** | Microcontroller for LoRa reception & serial relay | Connected to PC via USB |
| **LoRa E32** | Long-range telemetry link | D10 → RX, D11 → TX via `SoftwareSerial` |
| **PC / Web UI Host** | Runs Chrome/Edge web dashboard | Communicates via Web Serial API |

---

### 🔥 Launch Pad Unit (Sensor & Control)

| Component | Role | Connection Details |
|------------|------|--------------------|
| **Arduino Uno** | Core controller for sensor input & actuator control | — |
| **LoRa E32** | Telemetry + command receiver | D10 → RX, D11 → TX |
| **HX711 Load Cell Amplifier** | Measures thrust | D2 → SCK, D3 → DOUT |
| **DHT22 Sensor** | Measures temperature & humidity | D6 → DATA |
| **Relay Module** | Ignition control circuit | D13 → Trigger |
| **Servo Motor** | Physical safety arm mechanism | D5 → PWM |

---

## 💻 Firmware & Software Stack

### 🧠 Arduino Dependencies

Install via **Arduino Library Manager**:

<h2 align="center">🧠 Arduino Dependencies</h2>

Install the following via **Arduino Library Manager**:

- HX711  
- DHT sensor library (by Adafruit)  
- Adafruit Unified Sensor  
- SoftwareSerial *(built-in)*  
- Servo *(built-in)*  

Each subsystem has independent firmware:

/LaunchPad/LaunchPad_Tx_D13.ino
/GroundStation/GroundStation_Receiver.ino

yaml
Copy code

---

<h2 align="center">🌐 Ground Station Web UI</h2>

A **Web Serial API–based dashboard** for real-time monitoring and control.

**Tech Stack**
- HTML5 + CSS3 + JavaScript  
- Chart.js → Live thrust visualization  
- FileSaver.js → CSV data export  
- Glassmorphic, responsive design  

**UI Files**
/web-ui/
├── index.html
├── style.css
└── script.js

yaml
Copy code

---

<h2 align="center">📡 Communication Protocol</h2>

**Telemetry (LP → GS)**  
Data transmitted every 100 ms as a comma-separated string:
Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z

yaml
Copy code

| Field | Description | Unit |
|--------|--------------|------|
| XX.XX | Thrust | N |
| YY.Y | Temperature | °C |
| ZZ.Z | Humidity | %RH |

---

**Control Commands (GS → LP)**

| Action | Command | Response | State |
|--------|----------|-----------|--------|
| Arm System | `'A'` | Servo → 170° | 🔒 Armed |
| Test Fire | `'T'` | Relay ON (50 ms) | Armed |
| Launch Fire | `'I'` | Relay ON (Continuous) | Armed |
| Disarm / Safe | `'S'` | Servo → 10° | 🟢 Safe |

---

<h2 align="center">🧩 Repository Structure</h2>

DhumketuX_DETS/
├── LaunchPad/
│ └── LaunchPad_Tx_D13.ino
├── GroundStation/
│ └── GroundStation_Receiver.ino
├── web-ui/
│ ├── index.html
│ ├── style.css
│ └── script.js
├── docs/
│ └── calibration_guide.md
└── README.md

yaml
Copy code
---

