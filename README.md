Perfect. You want a **developer-grade, authoritative `README.md`** for the **DhumketuX Engine Test System (DETS)** — a *complete, fail-safe, end-to-end technical document* for firmware engineers and system integrators.

Below is the **final, publication-ready README.md**, designed to be your **single-source technical reference** for deployment, debugging, and future scaling. It follows the structure and tone of professional embedded system documentation (e.g., SpaceX or BPS.space internal documentation style).

---

````markdown
<p align="center">
  <img src="https://github.com/DhumketuX/DhumketuX-DETS/assets/logo.png" width="180">
</p>

<h1 align="center">🚀 DhumketuX Engine Test System (DETS)</h1>

<p align="center">
  Bi-directional telemetry and ignition control system for static rocket engine testing — precision-engineered for reliability, real-time control, and safety.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Microcontroller-STM32%20Bluepill%20%2B%20Arduino%20Uno-blue?logo=stmicroelectronics" />
  <img src="https://img.shields.io/badge/Wireless-LoRa%20RA--02-green?logo=radio" />
  <img src="https://img.shields.io/badge/Telemetry-Full%20Duplex%20Binary-yellow" />
  <img src="https://img.shields.io/badge/License-MIT-lightgrey" />
</p>

---

## 🧭 1. Project Overview & Architecture

**Purpose:**  
The **DhumketuX Engine Test System (DETS)** is a *bi-directional telemetry and firing control platform* designed for **static rocket engine testing**.  
It provides **real-time thrust monitoring**, **environmental telemetry**, and **secure ignition triggering** under strict safety protocols.

### 🔧 System Architecture

| Unit | Hardware | Function |
|------|-----------|-----------|
| **Launch Pad Unit (LP)** | STM32 Bluepill + LoRa RA-02 (SPI) | High-speed data acquisition and ignition control |
| **Ground Station Unit (GS)** | Arduino Uno/Nano + LoRa RA-02 (SPI) | Command bridge, serial relay, and UI link |

The two units communicate via LoRa at **433 MHz**, using synchronized binary telemetry packets and ASCII command encoding.

---

## 🔌 2. Hardware & Wiring Matrix (Deployment Reference)

Below is the **consolidated, fail-proof wiring guide** for both units.

### 🛰️ Launch Pad Unit (LP – STM32 Bluepill)

| Component | Function | STM32 Pin | Notes |
|------------|-----------|------------|-------|
| **LoRa RA-02** | NSS / CS | PA4 | SPI chip select |
| | RST | PB12 | LoRa hardware reset |
| | DIO0 | PB13 | Interrupt (RxDone/TxDone) |
| | SCK | PA5 | SPI Clock |
| | MISO | PA6 | SPI MISO |
| | MOSI | PA7 | SPI MOSI |
| **HX711** | Load Cell DOUT | PB0 | Thrust sensor input |
| | Load Cell SCK | PB1 | 24-bit ADC clock |
| **DHT22** | Temperature/Humidity | PA8 | One-wire data line |
| **Ignition Relay** | Fire Command | PC13 | Active-LOW, 300 ms pulse |
| **Safety Servo** | Arm/Disarm | PA9 | PWM output |
| **Status Buzzer/LED** | Feedback | PB10 | Command + Launch indication |

---

### 🧭 Ground Station Unit (GS – Arduino Uno/Nano)

| Component | Function | Arduino Pin | Notes |
|------------|-----------|-------------|-------|
| **LoRa RA-02** | NSS / CS | D10 | SPI chip select |
| | RST | D9 | LoRa hardware reset |
| | DIO0 | D2 | Interrupt pin |
| | SCK | D13 | SPI Clock |
| | MISO | D12 | SPI MISO |
| | MOSI | D11 | SPI MOSI |
| **Status LED** | Link Indicator | D8 | Active HIGH |

⚠️ **Logic Level Warning:**  
The **LoRa RA-02** operates at **3.3 V logic**.  
When interfacing with 5 V Arduinos, **use a level shifter** or voltage divider on all SPI and control lines to prevent module damage.

---

## 🧩 3. Firmware and Dependencies

### Environment Setup

| Platform | Required Core | IDE Version |
|-----------|----------------|--------------|
| **Launch Pad (STM32)** | STM32Duino Core (v2.6.0+) | Arduino IDE v2.2.1 |
| **Ground Station (Arduino)** | Arduino AVR Boards (v1.8.6) | Arduino IDE v2.2.1 |

### Required Libraries

| Library | Author | Version | Applies To |
|----------|---------|----------|-------------|
| **LoRa** | Sandeep Mistry | v0.8.0 | Both |
| **SPI** | Arduino | Built-in | Both |
| **HX711** | Bogde | v1.2.3 | LP |
| **DHT sensor library** | Adafruit | v1.4.4 | LP |
| **Adafruit Unified Sensor** | Adafruit | v1.1.7 | LP |
| **Servo** | Arduino | Built-in | LP |

---

## 📡 4. Communication Protocol (Layer 1 & 2)

### 4.1. Wireless Layer

| Parameter | Value |
|------------|--------|
| **Frequency** | 433 MHz |
| **Sync Word** | 0x12 |
| **Bandwidth** | 125 kHz |
| **Coding Rate** | 4/5 |
| **Spreading Factor** | 7 |
| **Tx Power** | 20 dBm (max) |
| **Mode** | Packet Mode (Non-continuous) |

The LoRa link ensures reliable, low-latency transmission between LP and GS in open-field conditions up to several hundred meters.

---

### 4.2. Data Specification

#### Telemetry Packet (LP → GS)

Binary payload transmitted as packed struct:

```c
typedef struct {
    float thrust;          // Thrust (N)
    float temperature;     // Temperature (°C)
    float humidity;        // Humidity (%RH)
    bool  isArmedStatus;   // Armed or Disarmed
} Telemetry_t;
````

> **Note:** `float` used for size consistency (4 bytes).
> Ensure both LP and GS use identical struct alignment.

#### Serialized String (GS → PC)

The GS converts the binary payload into the following formatted ASCII string for Web UI display:

```
Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z
```

Example:

```
Thrust:45.23,Temp:31.7,Humi:58.6
```

---

## 🧠 5. Fail-Safe Operations & UI Integration

### 5.1. Command Table (PC → GS → LP)

| Command | Action | Effect                                |
| ------- | ------ | ------------------------------------- |
| **A**   | ARM    | Unlock servo, enable ignition circuit |
| **S**   | SAFE   | Lock servo, disable ignition          |
| **T**   | TEST   | Trigger immediate telemetry broadcast |
| **I**   | IGNITE | Fire relay for 300 ms pulse           |

All commands are single ASCII characters, parsed non-blockingly on the LP.

---

### 5.2. Safety Sequence

1. **Password Entry** on Web UI (auth gate).
2. **Send ‘A’** to ARM (servo unlocks, ignition circuit enabled).
3. **Send ‘I’** to launch (relay fires for 300 ms, confirmed via tone + LED).
4. **System auto-reverts** to SAFE mode post ignition.

---

### 5.3. Visual Feedback System

| Indicator             | Source   | Meaning                       |
| --------------------- | -------- | ----------------------------- |
| **GS Dot (Green)**    | Web UI   | Link Active (Telemetry OK)    |
| **GS Dot (Red)**      | Web UI   | Link Lost (No data for >2 s)  |
| **GS Dot (Blinking)** | Web UI   | T–30 Countdown active         |
| **LP LED Flash**      | Hardware | Command received successfully |
| **LP Buzzer Tone**    | Hardware | Launch sequence triggered     |

---

## 🧰 6. Repository Layout

```
DhumketuX_DETS/
├── LaunchPad_Unit/
│   ├── LaunchPad_Tx.ino
│   ├── telemetry.h
│   └── command_handler.cpp
├── GroundStation_Unit/
│   └── GS_Receiver.ino
├── Web_Dashboard/
│   └── index.html
├── docs/
│   ├── pinout_diagram.png
│   ├── comm_flow.png
│   └── safety_protocols.md
└── README.md
```

---

## ⚙️ 7. Operational Checklist

| Step | Action                                    | Tool                    |
| ---- | ----------------------------------------- | ----------------------- |
| 1    | Connect LP → Load cell, DHT, Relay, Servo | Wiring diagram          |
| 2    | Flash LP firmware via STLink              | STM32CubeProgrammer     |
| 3    | Flash GS firmware via USB                 | Arduino IDE             |
| 4    | Open Web Serial Dashboard                 | Chrome or Edge          |
| 5    | Select COM port & observe live telemetry  | —                       |
| 6    | Execute ARM → IGNITE sequence             | Web UI / Serial Monitor |

---

## 🧩 8. Known Stability Rules

* Use **dedicated 3.3 V rail** for LoRa (≥500 mA recommended).
* Keep **SPI wiring <15 cm** for stable comms.
* Never trigger ignition without ARM confirmation.
* Verify **load cell calibration** before static test runs.
* Disable USB serial during flight/stand-alone LP operation.

---

## 🧾 License

Licensed under the **MIT License**.
See [LICENSE](./LICENSE) for details.

---

<p align="center">
  🧠 Engineered with precision by <b>Lian Mollick</b>  
  <br>Part of the <b>DhumketuX Propulsion Research Program</b>  
  <br><i>"Reliability begins with discipline."</i>
</p>
```

---

Would you like me to generate a **“Web Dashboard Integration” section** next — with JavaScript snippets for parsing telemetry strings and triggering ARM/IGNITE commands from a browser? It would make the README fully interactive for lab deployment.
