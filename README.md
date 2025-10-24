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

```text
HX711
DHT sensor library (by Adafruit)
Adafruit Unified Sensor
SoftwareSerial (built-in)
Servo (built-in)
Each subsystem has independent firmware:

/LaunchPad/LaunchPad_Tx_D13.ino

/GroundStation/GroundStation_Receiver.ino

🌐 Ground Station Web UI
A Web Serial API–based dashboard for real-time monitoring and control.

Tech Stack:

HTML5 + CSS3 + JavaScript

Chart.js – Live thrust graph

FileSaver.js – CSV data export

Glassmorphic responsive design

UI Files:

bash
Copy code
/web-ui/
├── index.html
├── style.css
└── script.js
📡 Communication Protocol
🔁 Telemetry (LP → GS)
Data transmitted every 100 ms as a comma-separated, newline-terminated string:

ruby
Copy code
Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z
Field	Description	Unit
XX.XX	Thrust	N
YY.Y	Temperature	°C
ZZ.Z	Humidity	%RH

🧭 Control Commands (GS → LP)
Action	Command	Response	Safety State
Arm System	'A'	Servo → 170°	🔒 Armed
Test Fire	'T'	Relay ON (50 ms pulse)	Armed
Launch Fire	'I'	Relay ON (continuous)	Armed
Disarm / Safe	'S'	Servo → 10°	🟢 Safe

🧩 Repository Structure
pgsql
Copy code
DhumketuX_DETS/
├── LaunchPad/
│   └── LaunchPad_Tx_D13.ino
├── GroundStation/
│   └── GroundStation_Receiver.ino
├── web-ui/
│   ├── index.html
│   ├── style.css
│   └── script.js
├── docs/
│   └── calibration_guide.md
└── README.md
---

<h2 align="center">🔧 Setup & Operation Guide</h2>

### ⚙️ 1️⃣ Configure Hardware

1. Ensure both **LoRa E32** modules share identical parameters:
   - **Baud Rate:** `9600 bps`  
   - **Channel:** Same for both modules  
   - **Air Data Rate:** Same configuration on both ends  

2. Calibrate the **Load Cell** using the HX711 calibration sketch.  
   - Update the `CALIBRATION_FACTOR` value inside the Launch Pad firmware.  

---

### 💾 2️⃣ Upload Firmware

| Unit | File | Board | Port |
|------|------|--------|------|
| **Launch Pad** | `LaunchPad_Tx_D13.ino` | Arduino Uno | COMx |
| **Ground Station** | `GroundStation_Receiver.ino` | Arduino Uno | COMy |

> Upload each sketch separately using the **Arduino IDE**.

---

### 🖥️ 3️⃣ Run Ground Station UI

1. Open `index.html` in **Google Chrome** or **Microsoft Edge**.  
2. Click **“CONNECT GROUND STATION”** and select the correct serial port.  
3. Observe **live telemetry data** in the dashboard (Thrust, Temperature, Humidity).  
4. Operate in this recommended sequence:

   | Action | Command | Description |
   |--------|----------|-------------|
   | 🔒 **ARM SYSTEM** | `'A'` | Enables safety servo and prepares ignition |
   | ⚡ **TEST FIRE** | `'T'` | Sends a short ignition pulse (50 ms) |
   | 🚀 **LAUNCH FIRE** | `'I'` | Activates ignition continuously |
   | 🟢 **DISARM / SAFE** | `'S'` | Returns servo to safe state |

5. After the test, click **💾 “Save Data (CSV)”** to export all telemetry logs.

---

<h2 align="center">🧠 Safety Guidelines</h2>

- Perform tests **outdoors** in a secure, controlled area.  
- Keep all personnel **clear of the exhaust or blast radius**.  
- Always **DISARM** the system when idle or before handling the igniter.  
- Verify **servo interlock** before sending any fire command.  
- Use **separate power lines** for ignition and logic control to prevent interference.

---

<h2 align="center">🚀 Future Roadmap</h2>

✅ Planned Upgrades and Features:
- [ ] MQTT / WebSocket–based cloud telemetry  
- [ ] Automatic load cell calibration routine  
- [ ] Integrated pressure and flow sensor support  
- [ ] Cross-platform **Electron Dashboard App**  

---

<h2 align="center">👨‍💻 Contributing</h2>

We welcome contributions from the community!

**How to contribute:**
1. **Fork** this repository  
2. Create a new branch → `feature/your-feature-name`  
3. Commit your changes with **clear, descriptive messages**  
4. Open a **Pull Request** against the `main` branch  

---

<h2 align="center">🧾 License</h2>

This project is licensed under the **MIT License**.  
See the [LICENSE](./LICENSE) file for full details.

---

<p align="center">
  Developed with ❤️ by <b>Lian</b> <br>
  <sub>Firmware • Embedded Systems • Rocket Telemetry Enthusiast</sub>
</p>

---
