<p align="center">
  <img width="252" height="80" alt="DhumketuX Logo" src="https://github.com/user-attachments/assets/447263ab-24ea-4f26-ab0a-907d59a87e52" />
</p>

<h1 align="center">🚀 DhumketuX Engine Test System (DETS)</h1>

<p align="center">
  <b>Modular Bi-Directional Telemetry & Control Framework for Rocket Engine Static Testing</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Microcontroller-Arduino%20Uno-blue?logo=arduino&style=flat-square">
  <img src="https://www.google.com/search?q=https://img.shields.io/badge/Wireless-LoRa%2520RA--02-green%3Flogo%3Dwifi%26style%3Dflat-square">
  <img src="https://img.shields.io/badge/UI-Web%20Serial%20Dashboard-orange?logo=javascript&style=flat-square">
  <img src="https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square">
</p>

🌟 Overview

The DhumketuX Engine Test System (DETS) is a firmware-driven, modular telemetry and control framework built for safely monitoring, logging, and executing static fire tests of small-scale rocket propulsion units.

It features two wirelessly linked subsystems communicating via LoRa:

🛰️ Ground Station (GS): Browser-based command and visualization dashboard.

🔥 Launch Pad (LP): Real-time sensor acquisition and ignition control logic.

Key highlights

Safe, two-step ignition arming system

Real-time thrust, temperature & humidity data

Strictly C-style programming on the MCU (no String objects)

CSV logging for research and analysis

Glass-style responsive web interface

⚙️ System Architecture

🛰️ Ground Station Unit (Receiver & UI Host)

The GS firmware (GS_Receiver.ino) uses Hardware SPI for high-speed LoRa communication and acts as a reliable serial bridge.

Component

Role

Connection Details (Arduino Uno/Nano)

Arduino Uno

Microcontroller for LoRa reception & serial relay

Connected to PC via USB

LoRa RA-02 Module

Long-range telemetry link (Hardware SPI)

D10 → CS, D9 → RST, D2 → DIO0 (IRQ)

PC / Web UI Host

Runs Chrome/Edge web dashboard

Communicates via Web Serial API

🔥 Launch Pad Unit (Sensor & Control)

Component

Role

Connection Details

Arduino Uno

Core controller for sensor input & actuator control

—

LoRa Module

Telemetry + command transceiver

D10 → CS, D9 → RST, D2 → DIO0 (IRQ)

HX711 Load Cell Amplifier

Measures thrust

D2 → SCK, D3 → DOUT

DHT22 Sensor

Measures temperature & humidity

D6 → DATA

Relay Module

Ignition control circuit

D13 → Trigger

Servo Motor

Physical safety arm mechanism

D5 → PWM

💻 Firmware & Software Stack

🧠 Arduino Dependencies

Install the following via Arduino Library Manager:

LoRa (By Sandeep Mistry)

SPI (built-in)

HX711

DHT sensor library (by Adafruit)

Adafruit Unified Sensor

Servo (built-in)

Each subsystem has independent firmware:

/LaunchPad/LaunchPad_Tx_D13.ino
/GroundStation/GS_Receiver.ino


<h2 align="center">🌐 Ground Station Web UI</h2>

A Web Serial API–based dashboard for real-time monitoring and control.

Tech Stack

HTML5 + CSS3 + JavaScript

Chart.js → Live thrust visualization

FileSaver.js → CSV data export

Glassmorphic, responsive design

UI Files

/web-ui/
├── index.html
├── style.css
└── script.js


<h2 align="center">📡 Communication Protocol</h2>

Telemetry (LP $\rightarrow$ GS $\rightarrow$ PC)

Data is transmitted from the Launch Pad as a binary struct (Telemetry_t) for efficiency. The Ground Station firmware immediately serializes this binary data into the following comma-separated string format for the Web Serial UI:

Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z


Field

Description

Unit

XX.XX

Thrust

kPa (Kilopascals)

YY.Y

Temperature

°C

ZZ.Z

Humidity

%RH

Control Commands (PC $\rightarrow$ GS $\rightarrow$ LP)

Commands are sent from the Web UI to the Ground Station as single characters over the USB Serial link. The GS immediately transmits the single character via LoRa to the Launch Pad.

Action

Command

Target State

Arm System

'A'

🔒 Armed (Servo moves)

Test Fire

'T'

Relay ON (50 ms)

Launch Fire

'I'

Relay ON (Continuous)

Disarm / Safe

'S'

🟢 Safe (Servo returns)

<h2 align="center">🧩 Repository Structure</h2>

DhumketuX_DETS/
├── LaunchPad/
│ └── LaunchPad_Tx_D13.ino
├── GroundStation/
│ └── GS_Receiver.ino
├── web-ui/
│ ├── index.html
│ ├── style.css
│ └── script.js
├── docs/
│ └── calibration_guide.md
└── README.md


<h1 align="center">Made By Lian Mollick</h1>
