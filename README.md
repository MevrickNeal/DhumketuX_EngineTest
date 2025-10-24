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
🔧 Setup & Operation Guide
1️⃣ Configure Hardware
Ensure both LoRa E32 modules share identical parameters:

Baud: 9600 bps

Same Channel & Air Data Rate

Calibrate the Load Cell and set CALIBRATION_FACTOR in firmware.

2️⃣ Upload Firmware
Unit	File	Board	Port
Launch Pad	LaunchPad_Tx_D13.ino	Arduino Uno	COMx
Ground Station	GroundStation_Receiver.ino	Arduino Uno	COMy

Upload each sketch from Arduino IDE.

3️⃣ Run Ground Station UI
Open index.html in Chrome or Edge.

Click “CONNECT GROUND STATION” and select serial port.

Observe live telemetry.

Operate sequence:

🔒 ARM SYSTEM (‘A’)

⚡ TEST FIRE (‘T’) or LAUNCH (‘I’)

🟢 DISARM (‘S’) after completion.

💾 Click “Save Data (CSV)” to export telemetry log.

🧠 Safety Guidelines
Perform tests outdoors, in a controlled and secure zone.

Keep all personnel clear of the exhaust path.

Always disarm the system when idle.

Verify servo interlock before arming ignition.

Use a separate power line for ignition and logic systems.

🚀 Future Roadmap
 MQTT / WebSocket cloud telemetry

 Automatic load cell calibration

 Integrated pressure & flow sensors

 Cross-platform dashboard (Electron app)

👨‍💻 Contributing
Pull requests are welcome!
Please follow these steps:

Fork the repository

Create a new branch (feature/your-feature)

Commit changes with descriptive messages

Open a PR against main

🧾 License
Distributed under the MIT License.
See the LICENSE file for more information.

<p align="center"> Developed with ❤️ by <b>Lian</b> <br> <sub>Firmware • Embedded Systems • Rocket Telemetry Enthusiast</sub> </p> ```
