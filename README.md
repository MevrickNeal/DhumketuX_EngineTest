<p align="center"> <img width="252" height="80" alt="dhumketux-logo" src="https://github.com/user-attachments/assets/d4caefdf-5ff9-491e-a70a-736fbb335012" /> </p> <h1 align="center">🚀 DhumketuX Engine Test System (DETS)</h1> <p align="center"> A complete static rocket engine test stand control ecosystem with secure ignition logic, thrust measurement, and real-time UI monitoring. Built for reliability, maintained for teams. </p> <p align="center"> <img src="https://img.shields.io/badge/Microcontroller-STM32%20Bluepill%20(LaunchPad)%20|%20Arduino%20Uno%20(GS)-blue?logo=stmicroelectronics" /> <img src="https://img.shields.io/badge/Network-LoRa%20433%20MHz-green?logo=radio" /> <img src="https://img.shields.io/badge/Telemetry-ASCII%20Bi--Directional-orange" /> <img src="https://img.shields.io/badge/License-MIT-lightgrey" /> </p>
<img width="1875" height="908" alt="image" src="https://github.com/user-attachments/assets/24384edd-1439-4571-b1c5-bd823c030f97" />
🧭 1. System Purpose & Architecture

Goal: Execute safe static fire tests with continuous thrust streaming, verified arming, and a robust ignition path. The system splits into three cooperating parts:

Subsystem	Role
Launch Pad Unit (LP, STM32)	Reads the load cell via HX711, manages servo interlock, drives ignition relay, answers commands, streams telemetry
Ground Station (GS, Arduino)	Relays PC Web Serial commands to LoRa and forwards LP telemetry back to the PC
Web Dashboard (Browser)	Plots thrust, marks ignition and peak, shows state, records CSV, provides operator controls

High level dataflow

PC WebApp  <—USB—>  GS Arduino  <—LoRa 433—>  LP STM32  <—HX711—>  Load Cell
   |  commands                         | RF cmds         | acts on servo + relay
   |  telemetry display                | RF telemetry    | telemetry generation

🔌 2. Hardware Pinout, Electrical Notes, and Rationale
2.1 Launch Pad Unit — STM32 Bluepill
Component	STM32 Pin	Electrical Level	Why this pin	Function
HX711 DOUT	PB0	3.3 V	Stable GPIO, not shared with SPI	24-bit ADC data from load cell
HX711 SCK	PB1	3.3 V	Clean clocking on basic GPIO	ADC clock for HX711
LoRa RA-02 CS	PA4	3.3 V	Standard SPI CS on STM32	SPI chip select
LoRa RA-02 RST	PB12	3.3 V	Dedicated reset control	Radio reset for fault clear
LoRa RA-02 DIO0	PB13	3.3 V	External interrupt capable	RxDone or TxDone IRQ
LoRa RA-02 SCK	PA5	3.3 V	SPI clock	Radio SPI clock
LoRa RA-02 MISO	PA6	3.3 V	SPI input	Radio SPI MISO
LoRa RA-02 MOSI	PA7	3.3 V	SPI output	Radio SPI MOSI
Ignition Relay	PC13	3.3 V output to driver	PC13 supports sink behavior, reliable as digital output	Active LOW fire: 0 = fire, 1 = safe
Safety Servo PWM	PA3	3.3 V PWM	TIM2 channel yields steady PWM	0° SAFE, 15° ARMED
Buzzer or Status LED	PB10	3.3 V	Simple audible or visual feedback	Tones for actions
DHT22 (optional)	PB15	3.3 V	One-wire capable	Environmental sensing

Power rails

STM32 and LoRa run at 3.3 V.

Provide a clean 3.3 V rail with 500 mA headroom for LoRa bursts.

Servo power should be isolated where possible. Use separate 5 V rail for servo, share ground with STM32, and drive via proper PWM.

Relay coil must be driven through a transistor or driver module with flyback diode.

Why PC13 for relay

Easy to reason about an Active-LOW fire line on a standalone output.

Keeps logic simple: write LOW for a short pulse to fire, return HIGH to safe.

Servo angles

SAFE: 0°

ARMED: 15°

Values are small by design to work as a mechanical interlock latch rather than a wide sweep.

2.2 Ground Station — Arduino Uno or Nano
Component	Arduino Pin	Voltage	Function	Note
LoRa RA-02 CS	D10	3.3 V	SPI CS	Must level shift from 5 V Arduino
LoRa RA-02 RST	D9	3.3 V	Radio reset	Level shift required
LoRa RA-02 DIO0	D2	3.3 V	IRQ input	Use level shifter or divider
LoRa RA-02 SCK	D13	3.3 V	SPI SCK	HW SPI, level shift recommended
LoRa RA-02 MISO	D12	3.3 V	SPI MISO	MISO can feed 5 V input, still recommend logic safe
LoRa RA-02 MOSI	D11	3.3 V	SPI MOSI	Level shift required
Status LED	D8	5 V	GS link indicator	Simple operator feedback

Level shifting is mandatory on Arduino outputs into the 3.3 V LoRa module. Use MOSFET level shifters or resistor dividers for SPI and control lines.

🧩 3. Firmware Architecture and Coding Guide

The codebase is split into focused modules to reduce coupling.

LaunchPad_Unit/
  launchPad.ino   - main loop, state machine, command parser, ignition pulse
  scale.ino       - HX711 setup, calibration read, kg output
  LoRa.ino        - radio setup, packets send/receive, ISR handler
  push.ino        - optional local button handling for maintenance
  buzz.ino        - tones or LED blink feedback

GroundStation_Unit/
  gndStation.ino  - serial <-> LoRa bridge for commands
  incoming.ino    - RF telemetry parsing, forward to PC serial

Web_Dashboard/
  index.html      - Web Serial client, plotting, state UI, CSV log

3.1 LP state machine
[SAFE]
  |  on ARM command
  v
[ARMED]  (servo 15°)
  | on IDLE -> SAFE (servo 0°)
  | on LAUNCH -> FIRE pulse then SAFE
  v
[FIRE]
  - drive relay LOW ~300 ms
  - buzzer tone
  - auto return to SAFE (servo 0°)
  v
[SAFE]


Active LOW ignition: write PC13 LOW for about 300 ms, then back HIGH.

After firing completes, LP automatically returns to SAFE, servo back to 0°.

3.2 Telemetry production

Load cell reads in kg.

Output format is ASCII line based so parsers stay simple.

Two modes for OK vs NO are supported based on your direction:

Mode A, manual calibration: LP sends OK,kg only after operator or routine calibration finishes.

Mode B, stability based: LP sends OK,kg when the values are stable within a tolerance window for a defined time span, else NO,kg.

You can keep both behaviors in code by guarding OK output behind either a calibration flag or a stability estimator.

3.3 Recommended telemetry cadence

Chosen behavior: continuous stream from LP every 100 ms while system is live.

GS may still issue CHECK to provoke an immediate sample when needed.

UI declares link lost if no new lines arrive in about 2 seconds.

This pattern gives smooth charts and keeps latency low.

3.4 Command parser on LP

Recognized ASCII commands: CHECK, ARM, IDLE, LAUNCH

Unknown commands should be ignored.

Echoing short acknowledgements like ARMED, IDLE, LAUNCH helps the UI lock state.

3.5 Safety enforcement in code

Servo must be at 15° only while ARMED.

Servo must be 0° in SAFE or any error.

Relay can only be driven when internal state is ARMED.

After LAUNCH pulse completes, force SAFE and restore servo to 0°.

Any radio or sensor fault pushes state to SAFE.

📡 4. Radio and Protocol Details
4.1 LoRa RF settings
Parameter	Value
Frequency	433 MHz
Sync Word	0x12
Bandwidth	125 kHz
Coding Rate	4/5
Spreading Factor	7
Tx Power	up to 20 dBm

Short packets, ASCII payload, one line per message, newline terminated. This keeps Arduino and browser parsing simple.

4.2 Command messages, GS to LP
Message	Purpose	Typical LP response
CHECK	Ask for one immediate measurement	OK,12.34 or NO,12.34
ARM	Interlock open, servo to 15°	ARMED
IDLE	Interlock closed, servo to 0°	IDLE
LAUNCH	Fire sequence	LAUNCH then auto SAFE internally
4.3 Telemetry messages, LP to GS
Format	Meaning	Notes
OK,<kg>	Valid, calibrated or stable kg reading	UI converts kg to N with 9.81
NO,<kg>	Reading available but not yet OK	Still charted, flagged non-cal
ARMED or IDLE or LAUNCH	State hints	Optional but useful for UI sync

Unit conversion in UI: Newtons = kilograms × 9.81

📊 5. Web Dashboard Behavior

Plots Thrust (N) vs Time in real time.

Displays Max Thrust, Ignition time, Time to Peak.

Detects ignition when thrust crosses a threshold.

Rolling buffer of about 300 points.

CSV export includes time, thrust, state.

Replay mode lets you load any prior CSV to study the burn.

Link indicators

GS dot green when serial is live.

LP dot green when new telemetry appears within 2 seconds.

If stream pauses longer, UI shows LINK LOST until data resumes.

Camera

Off by default.

User can enable or disable from the UI.

When enabled, browser asks permission and shows the feed near the log.

🧪 6. Operator Workflow

Short checklist for field runs:

Inspect wiring and strain relief on load cell.

Verify relay driver, flyback protection, and that the line idles HIGH.

Connect LP and GS power.

Open Web Dashboard in Chrome, then connect to GS serial.

Press CHECK and confirm numeric readings appear.

Press ARM, verify servo moves to 15°.

Initiate launch sequence from the UI.

Observe ignition, thrust rise, plateau, shutdown.

LP returns to SAFE and servo returns to 0° by design.

Save CSV for records.

Distance from engine: at least 15 m for small motors. Use shields and eye protection. Keep fire control equipment ready.

🛠 7. Developer Guide: How to Modify Safely
7.1 Changing servo angles

In launchPad.ino or the servo helper:

// SAFE
servo.write(0);       // 0°
// ARMED
servo.write(15);      // 15°


If using microseconds API:

// map degrees to µs based on your servo
servo.writeMicroseconds(1000); // SAFE
servo.writeMicroseconds(1100); // ARMED ~15°

7.2 Changing ignition pulse duration

In the fire routine:

digitalWrite(IGNITION_PIN, LOW);   // Active LOW, fire
delay(300);                        // pulse ms
digitalWrite(IGNITION_PIN, HIGH);  // back to safe


Do not exceed the relay or igniter current rating. Keep pulse short and sufficient.

7.3 Adjusting telemetry rate

In the main loop or telemetry task:

const uint16_t TELEMETRY_INTERVAL_MS = 100;
// publish OK,kg or NO,kg every interval


The UI link timeout is about 2000 ms, so keep the stream more frequent than that.

7.4 Editing HX711 scaling

Update scale factor or calibration constants in scale.ino. Always verify with known weights and record the calibration date in the repo history.

7.5 RF changes

If you alter bandwidth or SF in LoRa.ino, mirror the same on GS. Keep packet sizes small. Maintain the same ASCII framing to avoid breaking the browser UI.

7.6 Adding new sensors

Choose free pins and document in the pin table.

Publish new values in separate lines or append to a comma string with a clear prefix.

Update the Web UI parser accordingly.

🧰 8. Troubleshooting and Fault Diagnosis

No telemetry on UI

Check GS serial port is connected.

Confirm GS LED is blinking or solid to indicate data.

Ensure LP is powered and LoRa antenna attached.

Reduce distance or obstacles between GS and LP.

Try CHECK command and watch the debug log.

Relay not firing

Confirm IGNITION pin idles HIGH and goes LOW during fire pulse.

Verify driver transistor orientation and flyback diode on coil.

Measure coil voltage during fire with a meter.

Ensure system is ARMED before sending LAUNCH.

Servo not moving

Confirm PWM pin configuration and 5 V supply for servo.

Check ground is common between servo and STM32.

Try a simple servo sweep test sketch.

Load cell reads zero or noisy

Reseat HX711 wires: E+, E−, A+, A−.

Add shielding or twisted pairs for A+ and A−.

Recalibrate and wait for stability before tests.

Random link loss

Provide clean 3.3 V, decouple near LoRa module.

Keep SPI lines short and neat.

Use a proper 433 MHz antenna with ground plane if possible.

📁 9. Repository Layout
DhumketuX_DETS/
├── LaunchPad_Unit/
│   ├── launchPad.ino      // state machine, command parsing, ignition
│   ├── scale.ino          // HX711 read, kg output, calibration
│   ├── buzz.ino           // tones, feedback
│   ├── push.ino           // local pushbutton handlers
│   └── LoRa.ino           // radio setup, send/receive, IRQ
├── GroundStation_Unit/
│   ├── gndStation.ino     // PC serial bridge, uplink commands
│   └── incoming.ino       // downlink telemetry to PC serial
├── Web_Dashboard/
│   └── index.html         // WebSerial, charts, logging, camera
└── README.md

🧾 10. License

MIT. Build, learn, iterate, share responsibly.

<p align="center"> 🧠 Engineered with precision by <b>Lian Mollick</b><br> Proudly part of the <b>DhumketuX Propulsion Research Program</b><br><br> 👑 Special Gratitude to <b>Fazle Elahi Tonmoy Bhai</b><br> For mentoring, debugging, and spending countless hours so that thrust curves look beautiful and test stands stay safe.<br><br> <i>"Reliability begins with discipline."</i> </p>
Quick Ops Card for Oli Bhai

Connect WebApp to GS, press CHECK, ensure numbers move.

Press ARM, verify servo goes to 15 degrees.

Run launch sequence, watch thrust chart, export CSV.

LP auto returns to SAFE after fire.

If anything feels odd, hit SAFE and step back.
