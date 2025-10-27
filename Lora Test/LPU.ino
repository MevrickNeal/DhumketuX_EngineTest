/**
 * DhumketuX LaunchPad Unit (LPU) — STM32 Bluepill
 * Paired with your current GS and Web UI (ASCII CSV, 5 fields)
 * CSV: thrust(N),temp(C),humi(%),uptime_ms,state(0=SAFE,1=ARMED,2=COUNTDOWN,3=IGNITION)
 */

#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include <DHT.h>
#include <HX711.h>
#include <math.h>

// ---------------- I. Pin map (Bluepill) ----------------
#define LORA_CS_PIN         PA4
#define LORA_RST_PIN        PA3
#define LORA_DIO0_PIN       PA2

#define LOADCELL_DOUT_PIN   PB0
#define LOADCELL_CLK_PIN    PB1

#define SERVO_PIN           PA9          // timer pin suitable for Servo on Bluepill core
#define IGNITION_RELAY_PIN  PC13         // active LOW to fire
#define ALERT_PIN           PB10         // buzzer or LED (tone capable preferred)
#define DHT_PIN             PA8
#define DHT_TYPE            DHT22

// ---------------- II. Radio and timings ----------------
#define LORA_FREQ_HZ        433E6        // LoRa.begin expects Hz
#define TELEMETRY_INTERVAL  100UL        // ms
#define SENSOR_READ_INTERVAL 1000UL      // ms
#define IGNITION_PULSE_MS   300UL        // ms, active LOW
#define HANDSHAKE_BEEP_MS   300UL

// Load cell
#define CALIBRATION_FACTOR  -400.0f      // adjust to your calibration

// Servo angles
#define SERVO_LOCK_ANGLE    0            // Safe
#define SERVO_ARM_ANGLE     90           // Armed

// ---------------- III. State machine ----------------
enum SystemState_t {
  STATE_DISARMED = 0,
  STATE_ARMED    = 1,
  STATE_COUNTDOWN= 2,
  STATE_IGNITION = 3
};

// ---------------- IV. Globals ----------------
Servo safetyServo;
DHT dht(DHT_PIN, DHT_TYPE);
HX711 LoadCell;

SystemState_t systemState = STATE_DISARMED;

float tx_thrust = 0.0f;
float tx_temp   = 0.0f;
float tx_humi   = 0.0f;

unsigned long lastTelemetryTx   = 0;
unsigned long lastSensorRead    = 0;
unsigned long sequenceStartTime = 0;   // used for countdown and ignition pulse
unsigned long lastToneToggle    = 0;

char txStringBuffer[96];

// Debounce for handshake spam
unsigned long lastZTime = 0;
const unsigned long Z_DEBOUNCE_MS = 1000;

// ---------------- V. Helpers ----------------
float readLoadCell() {
  return LoadCell.get_units(1);   // one sample average to keep latency low
}

void safeTone(unsigned freq, unsigned dur) {
  // Some cores do not allow tone on any pin. If your core supports tone() on PB10 this works.
  tone(ALERT_PIN, freq, dur);
}

void setSafety(bool lockState) {
  if (!safetyServo.attached()) {
    safetyServo.attach(SERVO_PIN);
  }
  safetyServo.write(lockState ? SERVO_LOCK_ANGLE : SERVO_ARM_ANGLE);
  Serial.print("Safety: ");
  Serial.println(lockState ? "LOCKED" : "UNLOCKED");
}

void readEnvironmentSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) tx_temp = t;
  if (!isnan(h)) tx_humi = h;
}

// ---------------- VI. Telemetry ----------------
void transmitTelemetry() {
  tx_thrust = readLoadCell();

  int armedState = (systemState == STATE_IGNITION) ? 3 :
                   (systemState == STATE_COUNTDOWN) ? 2 :
                   (systemState == STATE_ARMED)     ? 1 : 0;

  // thrust,temp,humi,uptime,state
  snprintf(txStringBuffer, sizeof(txStringBuffer),
           "%.2f,%.1f,%.1f,%lu,%d\r\n",
           tx_thrust, tx_temp, tx_humi, millis(), armedState);

  LoRa.beginPacket();
  LoRa.print(txStringBuffer);
  LoRa.endPacket(true);   // blocking is fine at 10 Hz telemetry
}

// ---------------- VII. Command handling ----------------
void handleCommand(char command) {
  unsigned long now = millis();

  switch (command) {
    case 'Z': { // handshake ping
      if (now - lastZTime < Z_DEBOUNCE_MS) break;   // ignore spam
      lastZTime = now;
      if (systemState == STATE_DISARMED) {
        safeTone(1000, HANDSHAKE_BEEP_MS);
        digitalWrite(ALERT_PIN, HIGH);
        delay(20);
        digitalWrite(ALERT_PIN, LOW);
        Serial.println("Handshake received (SAFE).");
      } // ignore Z in ARMED/COUNTDOWN/IGNITION
      break;
    }

    case 'A': { // arm
      if (systemState == STATE_DISARMED) {
        systemState = STATE_ARMED;
        setSafety(false);
        Serial.println("CMD A: ARMED. Servo moved to ARM.");
      }
      break;
    }

    case 'S': { // safe
      if (systemState != STATE_DISARMED) {
        systemState = STATE_DISARMED;
        setSafety(true);
        digitalWrite(IGNITION_RELAY_PIN, HIGH);   // make sure OFF
        noTone(ALERT_PIN);
        Serial.println("CMD S: DISARMED. Servo moved to SAFE.");
      }
      break;
    }

    case 'L': { // 30 s countdown to ignition
      if (systemState == STATE_ARMED) {
        systemState = STATE_COUNTDOWN;
        sequenceStartTime = now;
        lastToneToggle = now;
        Serial.println("CMD L: COUNTDOWN started (30 s).");
      }
      break;
    }

    case 'T': { // test pulse + tone
      if (systemState == STATE_ARMED) {
        systemState = STATE_IGNITION;
        sequenceStartTime = now;
        Serial.println("CMD T: TEST PULSE started.");
      }
      break;
    }
  }
}

void checkLoRaCommand() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  // Read first byte as command and flush the rest
  char command = (char)LoRa.read();
  while (LoRa.available()) LoRa.read();

  Serial.print("RX Command: ");
  Serial.println(command);

  handleCommand(command);
}

// ---------------- VIII. Timed sequences ----------------
void handleSequences(unsigned long now) {
  if (systemState == STATE_COUNTDOWN) {
    const unsigned long COUNTDOWN_MS = 30000UL;
    unsigned long elapsed   = now - sequenceStartTime;
    long remaining = (long)COUNTDOWN_MS - (long)elapsed;

    if (remaining <= 0) {
      // go to ignition pulse
      systemState = STATE_IGNITION;
      sequenceStartTime = now;
      return;
    }

    // Beep cadence accelerates in last 10 s
    unsigned long toneInterval = (remaining > 10000UL) ? 1000UL : 200UL;
    if (now - lastToneToggle >= toneInterval) {
      if (remaining > 50) safeTone((remaining > 10000UL) ? 500 : 1500, 100);
      lastToneToggle = now;
    }
  }
  else if (systemState == STATE_IGNITION) {
    // Drive relay active LOW for IGNITION_PULSE_MS
    if (now - sequenceStartTime < IGNITION_PULSE_MS) {
      digitalWrite(IGNITION_RELAY_PIN, LOW);
      safeTone(3000, 80);
      digitalWrite(ALERT_PIN, HIGH);
    } else {
      digitalWrite(IGNITION_RELAY_PIN, HIGH);
      noTone(ALERT_PIN);
      // After test pulse or launch, auto safe
      systemState = STATE_DISARMED;
      setSafety(true);
      Serial.println("Ignition pulse complete. Auto DISARM.");
    }
  }
}

// ---------------- IX. Setup and loop ----------------
void setup() {
  pinMode(ALERT_PIN, OUTPUT);
  pinMode(IGNITION_RELAY_PIN, OUTPUT);
  digitalWrite(IGNITION_RELAY_PIN, HIGH);    // idle HIGH
  digitalWrite(ALERT_PIN, LOW);

  Serial.begin(115200);
  delay(50);

  // Sensors
  LoadCell.begin(LOADCELL_DOUT_PIN, LOADCELL_CLK_PIN);
  LoadCell.set_scale(CALIBRATION_FACTOR);
  LoadCell.tare();

  dht.begin();
  setSafety(true);

  // LoRa radio
  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQ_HZ)) {
    Serial.println("FATAL: LoRa init failed");
    while (1) { delay(100); }
  }
  // Align with GS and Web
  LoRa.setSpreadingFactor(10);         // SF10
  LoRa.setCodingRate4(5);              // 4/5
  LoRa.setSignalBandwidth(125E3);      // 125 kHz
  LoRa.setSyncWord(0x34);              // match GS
  LoRa.setTxPower(17);                 // adjust if needed

  // First sensor snapshot
  readEnvironmentSensors();

  Serial.println("\n*** LPU ready. Waiting for commands. ***");
}

void loop() {
  unsigned long now = millis();

  // Sequences
  handleSequences(now);

  // Sensors at 1 Hz
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readEnvironmentSensors();
  }

  // Telemetry at 10 Hz
  if (now - lastTelemetryTx >= TELEMETRY_INTERVAL) {
    lastTelemetryTx = now;
    transmitTelemetry();
  }

  // Commands
  checkLoRaCommand();
}
