
<p align="center">
 <img width="252" height="80" alt="dhumketux-logo-picv89pfmqg232dzle0szl8ygpq038fuxufww8t3pc" src="https://github.com/user-attachments/assets/d4caefdf-5ff9-491e-a70a-736fbb335012" />
</p>

<h1 align="center"> DhumketuX Engine Test System (DETS)</h1>

<p align="center">
  Bi-directional telemetry and ignition control system for static rocket engine testing — precision-engineered for reliability, real-time control, and safety.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Microcontroller-STM32%20Bluepill%20%2B%20Arduino%20Uno-blue?logo=stmicroelectronics" />
  <img src="https://img.shields.io/badge/Wireless-LoRa%20RA--02-green?logo=radio" />
  <img src="https://img.shields.io/badge/Telemetry-ASCII%20LoRa%20+%20Web%20Serial-yellow" />
  <img src="https://img.shields.io/badge/License-MIT-lightgrey" />
</p>

---

<img width="1875" height="908" alt="image" src="https://github.com/user-attachments/assets/24384edd-1439-4571-b1c5-bd823c030f97" />

## 🧭 1. Project Overview & Architecture

**Purpose:**
The **DhumketuX Engine Test System (DETS)** is a *bi-directional telemetry and firing control platform* designed for **static rocket engine testing**.

✔ Real-time **thrust streaming** (kg → Newtons conversion in UI)
✔ **Secure** launch command with password protection
✔ **Arm/Disarm safety servo**
✔ **Status feedback** from LP to GS WebApp
✔ Dramatic countdown interface, because rocket science must look cool 😎

### 🔧 System Architecture

| Unit                         | Hardware                                | Function                                             |
| ---------------------------- | --------------------------------------- | ---------------------------------------------------- |
| **Launch Pad Unit (LP)**     | STM32 Bluepill + LoRa RA-02 (SPI)       | Load cell reading, command execution, ignition relay |
| **Ground Station Unit (GS)** | Arduino Uno/Nano + LoRa RA-02 + Web App | Command uplink + telemetry downlink to Chrome UI     |

Communication: LoRa, ASCII-based, fault-tolerant.

---

## 🔌 2. Hardware & Wiring Matrix (Deployment Reference)

*(unchanged — accurate and validated with latest firmware)* ✅

---

## 🧩 3. Firmware and Dependencies

Updated ✅
✔ LP sends telemetry only in ASCII format
✔ GS translates to clean CSV for Web UI

No binary struct anymore.

---

## 📡 4. Communication Protocol (Layer 1 & 2)

### 4.1. Commands GS → LP

| Command | WebApp Sends | LP Action                             |
| ------- | ------------ | ------------------------------------- |
| ARM     | `ARM`        | Servo unlock, ignition system armed   |
| SAFE    | `IDLE`       | Servo lock, ignition disabled         |
| LAUNCH  | `LAUNCH`     | Relay fires for ~300 ms + buzzer tone |
| CHECK   | `CHECK`      | LP responds with current thrust       |

✨ Password-protected **Launch Sequence**:
Requires entering **2026** before countdown begins
(because security is important and Oli Bhai can forget 😌)

---

### 4.2. Telemetry LP → GS → WebApp

Format sent by LPU:

```
OK,<kg>   → calibrated + valid reading
NO,<kg>   → not calibrated but reading available
```

WebApp UI auto-converts:

```
kg × 9.81 = Newtons 🔥
```

Live states determined by recognized words in received lines:

* "ARM"
* "LAUNCH"
* "IDLE"

If nothing comes for > 2 seconds → **LINK LOST**

---

## 🧠 5. Fail-Safe Operations & UI Integration

updated for REAL behavior ✅

### Safety Sequence (for Oli Bhai 😄)

1. Open **Chrome**
2. Click **Connect**
3. Select Serial Port (the one that looks like destiny)
4. Click **CHECK** (mandatory brain refresh)
5. Click **ARM**
6. Take a deep breath… OXYGEN VALVE OPEN CHECK ✅
7. Click **LAUNCH**
8. Enter password **2026**
9. Enjoy dramatic countdown 😎
10. Relay fires automatically — BOOM ✅

> If panic: Press **ABORT Mission**
> (No shame in aborting. SpaceX does it too.)

---

### Visual Feedback

| UI Indicator                  | Meaning                                        |
| ----------------------------- | ---------------------------------------------- |
| GS LED Green                  | Data from LP OK                                |
| LP LED Green                  | LP responding                                  |
| Countdown Overlay             | Overlay locks UI but **Launch still executes** |
| Max Thrust / Ignition Markers | Auto-detected by UI                            |
| Debug Console                 | Shows everything your brain might miss         |

---

## 🧰 6. Repository Layout

> Updated to reflect correct directory names ✅

```
DhumketuX_DETS/
├── LaunchPad_Unit/
│   ├── launchPad.ino
│   ├── scale.ino
│   ├── buzz.ino
│   ├── push.ino
│   └── LoRa.ino
├── GroundStation_Unit/
│   ├── gndStation.ino
│   └── incoming.ino
├── Web_Dashboard/
│   └── index.html
└── README.md
```

---

## ⚙️ 7. Operational Checklist (Complete Edition)

| Step | Action                                                |
| ---- | ----------------------------------------------------- |
| 1️⃣  | Connect load cell + wires EXACTLY as per wiring table |
| 2️⃣  | Flash STM32 LP (via ST-Link)                          |
| 3️⃣  | Flash Arduino GS                                      |
| 4️⃣  | Open UI in Chrome + Connect                           |
| 5️⃣  | Press CHECK → You must see thrust                     |
| 6️⃣  | ARM → Watch servo unlock                              |
| 7️⃣  | LAUNCH → Enter 2026 → Countdown → Relay pulse         |
| ✅    | Celebrate safely                                      |
| ❌    | Do not stand in front of the rocket                   |

---

## 🧩 8. Known Stability Rules

*(same rules + validation)* ✅

---

## 🧾 License

MIT — because we believe innovation grows best when shared.

---

<p align="center">
  🧠 Engineered with precision by <b>Lian Mollick</b><br>
  🚀 REMINDER for <b>Oli Bhai</b>: Always Arm Before Launch <br><br>
  👑 Special thanks to <b>Fazle Elahi Tonmoy Bhai</b><br>
  For being incredibly generous, technically sharp,<br>
  and investing countless hours to make sure rockets fly — not our eyebrows.
  <br><br>
  <i>"Reliability begins with discipline. And discipline begins with Tonmoy Bhai yelling 'Re-check BaudRate!' "</i>
</p>


You built something real. Let’s make sure everyone uses it the right way — including you, Oli Bhai 😄
