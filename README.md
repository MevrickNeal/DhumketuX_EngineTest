<p align="center">
  <img width="252" height="80" src="https://github.com/user-attachments/assets/d4caefdf-5ff9-491e-a70a-736fbb335012" alt="DhumketuX Logo">
</p>

<h1 align="center">DhumketuX Engine Test System (DETS)</h1>

<p align="center">
Bi-directional telemetry and ignition-control system for static rocket engine tests
<br>
High-reliability. Real-time. Safety-first.
</p>

<p align="center">
<img src="https://img.shields.io/badge/Launch Pad-STM32 Bluepill-blue">
<img src="https://img.shields.io/badge/Ground Station-Arduino Uno/Nano-green">
<img src="https://img.shields.io/badge/Wireless-LoRa RA--02-yellow">
<img src="https://img.shields.io/badge/Status-Active--Low Ignition-critical">
</p>

---

<img width="900" alt="UI Preview" src="https://github.com/user-attachments/assets/24384edd-1439-4571-b1c5-bd823c030f97">

---

## 1. System Overview

DETS is designed by **DhumketuX Propulsion Research Program** to safely manage:

| Feature | Capability |
|--------|------------|
| Thrust Live Telemetry | Load Cell + HX711 (kg → N conversion in UI) |
| Temperature + Humidity | DHT22 |
| Ignition Control | **Active-LOW** Relay (0 = FIRE) |
| Safety Interlock | Servo-driven physical interlock |
| Wireless Communication | LoRa 433 MHz |
| Redundant Safety | Automatic SAFE after firing |

### System Block Diagram
📌 *(To be inserted: `docs/system_block.svg`)*

---

## 2. Hardware Wiring (Launch Pad Unit – STM32 Bluepill)

| Component | STM32 Pin | Purpose | Notes |
|----------|-----------|---------|------|
| HX711 DOUT | **PB0** | Load Cell | Force in KG |
| HX711 SCK | **PB1** | ADC Clock | |
| DHT22 | **PB15** | Environmental data | |
| Relay (Ignition) | **PC13** | 🔥 Ignition Command | **Active-LOW** |
| Safety Servo | **PA3** | 0° SAFE → 15° ARMED | |
| LoRa NSS | **PA4** | SPI CS | |
| LoRa SCK | **PA5** | SPI Clock | |
| LoRa MISO | **PA6** | SPI MISO | |
| LoRa MOSI | **PA7** | SPI MOSI | |
| LoRa RST | **PB12** | Reset | |
| LoRa DIO0 | **PB13** | IRQ | |
| Status Buzzer | **PB10** | Audible alerts | |

📌 Servo angles:  
SAFE = **0°**  
ARMED = **15°**

📌 Relay logic:  
HIGH = Safe (Open)  
LOW = 🔥 Fire

---

## 3. Hardware Wiring (Ground Station – Arduino Uno/Nano)

| Component | Pin | Notes |
|----------|-----|------|
| LoRa NSS | D10 | |
| LoRa RST | D9 | |
| LoRa DIO0 | D2 | Interrupt |
| LoRa SCK | D13 | |
| LoRa MISO | D12 | |
| LoRa MOSI | D11 | |
| Status LED | D8 | GS Link |

⚠ All LoRa I/O is **3.3 V**  
→ Level shifting recommended with Arduino

---

## 4. Communication & Data Format

| Direction | Format | Example |
|----------|--------|---------|
| GS → LP | ASCII Commands | `A`, `S`, `T`, `L` |
| LP → GS | CSV Telemetry | `12.5,29.3,68.4,1` |

### Telemetry Mapping
<thrust_kg> , <temp_C> , <humidity_%> , <LP_state>

yaml
Copy code

🔹 Thrust converted in UI:  
**Kg × 9.81 = N**  

---

## 5. LP State Machine

| State Code | Name | UI Indication |
|-----------:|------|---------------|
| 0 | SAFE | System Locked |
| 1 | ARMED | Ignition Enabled |
| 2 | COUNTDOWN | T-XX Display |
| 3 | IGNITION | Burn in progress |

📌 Automatically returns to SAFE after ignition.

---

## 6. Web Mission Control UI

| Control | Serial Command | Target |
|--------|----------------|--------|
| ARM | `A` | LP servo → ARMED |
| SAFE | `S` | LP immediate safe |
| TEST PULSE | `T` | Small procedural load |
| LAUNCH | `L` | Ignition (300ms LOW) |

### UI Flow Diagram
📌 *(To be inserted: `docs/mission_flow.svg`)*

---

## 7. File Structure

DhumketuX_DETS/
├─ firmware/
│ ├─ LaunchPad/
│ │ ├─ launchPad.ino
│ │ ├─ scale.ino
│ │ ├─ LoRa.ino
│ │ ├─ push.ino
│ │ └─ buzz.ino
│ └─ GroundStation/
│ ├─ gndStation.ino
│ └─ incoming.ino
├─ WebUI/
│ └─ index.html
└─ README.md

yaml
Copy code

---

## 8. Operator Workflow (Since Oli Bhai Will Forget 😄)

> Written lovingly so **Oli Bhai** never calls Tonmoy Bhai at 3 AM again.

1️⃣ Power LP + GS  
2️⃣ Connect browser → GS over Web Serial  
3️⃣ Confirm live telemetry updates  
4️⃣ Hit **ARM**  
 Servo moves to **15°**  
5️⃣ Press **Launch**  
 Countdown overlay → then ignition  
6️⃣ System falls back to SAFE automatically

✅ If anything feels wrong → Press **SAFE** immediately

---

## 9. Troubleshooting & Safety Notes

| Symptom | Cause | Fix |
|--------|------|-----|
| Link Green but LP Red | No telemetry | Check primary power to Bluepill |
| Fuse not firing | Relay stayed HIGH | Check Active-LOW wiring & continuity |
| Thrust weird | Load cell unstable | Re-calibrate HX711 |
| Countdown freezes UI | Browser permissions | Refresh + Reconnect |

### ABSOLUTE SAFETY RULES
✔ Confirm **SAFE** before powering igniter  
✔ Rehearse abort call before countdown  
✔ LP must be physically protected (blast box)

---

## 10. Credits

**Lead Engineer:** Nahiyan Al Rahman (Oli)  
**Co-Engineer:** Lian Mollick  
**Special Thanks:** Fazle Elahi Tonmoy Bhai  
*(For being kind enough to tolerate endless late-night debugging)*

---

## License

MIT License  
© 2025 DhumketuX Propulsion Research Program
