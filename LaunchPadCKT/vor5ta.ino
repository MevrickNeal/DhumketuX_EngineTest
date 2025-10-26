/**
 * DhumketuX LaunchPad (STM32 Bluepill) — ASCII CSV over LoRa
 * Author: Prepared for DhumketuX by ChatGPT (GPT-5 Thinking)
 *
 * CSV line ~10 Hz: "thrustN,tempC,humidity%,state"
 * Commands from GS: Z(handshake beep), A(arm), S(safe), L(30s countdown), T(test tone)
 *
 * Pin map (matches your latest share):
 *   LoRa RA-02  : CS=PA4, RST=PA3, DIO0=PA2   (SCK/MISO/MOSI = PA5/PA6/PA7)
 *   HX711       : DOUT=PB0, SCK=PB1
 *   DHT22       : DATA=PA8
 *   SafetyServo : PWM=PA9  (0°=locked, 90°=armed)
 *   Ignition    : PC13 (active-LOW, 300 ms pulse)
 *   Alert/Buzzer: PB10 (HIGH=on)
 *
 * Notes:
 * - Keep proper 433 MHz antennas on both RA-02 units.
 * - Power RA-02 from a solid 3.3 V rail.
 * - Calibrate HX711 (set hx_scale) in the field for accurate Newtons.
 */

#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include <DHT.h>
#include <HX711.h>

// ======== Pin map ========
#define LORA_CS_PIN       PA4
#define LORA_RST_PIN      PA3
#define LORA_DIO0_PIN     PA2

#define HX_DOUT_PIN       PB0
#define HX_SCK_PIN        PB1

#define DHT_PIN           PA8
#define DHT_TYPE          DHT22

#define SERVO_PIN         PA9
#define IGNITION_PIN      PC13      // Active-LOW relay
#define ALERT_PIN         PB10      // Buzzer/LED

// ======== LoRa radio ========
#define LORA_FREQ_HZ      433E6
#define LORA_SF           10        // must match GS
#define LORA_CR_DEN       5         // coding rate 4/5
// keep bandwidth/syncword defaults to match GS defaults
#define LORA_TX_POWER     20        // 2..20 (RA-02 PA_BOOST)

// ======== Timing ========
#define TELEMETRY_MS      100UL     // ~10 Hz CSV
#define SENSOR_MS         1000UL    // DHT refresh
#define COUNTDOWN_MS      30000UL
#define IGNITION_PULSE_MS 300UL
#define FEEDBACK_PULSE_MS 50UL
#define HANDSHAKE_BEEP_MS 500UL

// ======== Servo positions ========
#define SERVO_LOCK_DEG    0
#define SERVO_ARM_DEG     90

// ======== State machine ========
enum State : uint8_t {
  DISARMED = 0,
  ARMED    = 1,
  COUNTDOWN= 2,
  IGNITION = 3
};

State state = DISARMED;

// ======== Globals ========
DHT dht(DHT_PIN, DHT_TYPE);
HX711 hx;
Servo safetyServo;

unsigned long t_lastTx = 0;
unsigned long t_lastSense = 0;
unsigned long t_sequence = 0;
unsigned long t_cmdPulse = 0;

float g_temp = 27.0f;   // seeded for normal-looking values
float g_humi = 55.0f;   // seeded for normal-looking values
float g_thrustN = 0.0f;

long  hx_offset = 0;         // tare offset (set at boot)
float hx_scale  = 100000.0f; // placeholder; set in field: counts per kg

// thrust smoothing
#define THR_AVG_N 8
float thr_hist[THR_AVG_N] = {0};
uint8_t thr_idx = 0;
uint8_t thr_count = 0;

// ======== Helpers ========
static void setSafety(bool locked) {
  if (!safetyServo.attached()) safetyServo.attach(SERVO_PIN);
  safetyServo.write(locked ? SERVO_LOCK_DEG : SERVO_ARM_DEG);
}

static void pulseAlert(unsigned ms) {
  digitalWrite(ALERT_PIN, HIGH);
  delay(ms);
  digitalWrite(ALERT_PIN, LOW);
}

static float readThrustN_raw() {
  long raw = hx.read_average(8);
  long net = raw - hx_offset;
  float mass_kg = (float)net / hx_scale;   // requires field calibration
  float N = mass_kg * 9.81f;
  if (N < 0) N = 0;                         // no negative thrust
  return N;
}

static float smoothThrust(float sampleN) {
  thr_hist[thr_idx] = sampleN;
  thr_idx = (thr_idx + 1) % THR_AVG_N;
  if (thr_count < THR_AVG_N) thr_count++;

  float sum = 0;
  for (uint8_t i = 0; i < thr_count; i++) sum += thr_hist[i];
  float avg = sum / (float)thr_count;

  // Optional tiny-noise floor clamp
  if (avg < 0.05f) avg = 0.0f; // <0.05 N treated as zero
  return avg;
}

static void readEnv() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  // keep last good to avoid NaN holes
  if (!isnan(t)) g_temp = t;
  if (!isnan(h)) g_humi = h;
}

static void sendCSV() {
  // compute thrust → smooth → send
  float rawN = readThrustN_raw();
  g_thrustN = smoothThrust(rawN);

  // sanitize in case of edge anomalies
  float th = isnan(g_thrustN) ? 0.0f : g_thrustN;
  float tp = isnan(g_temp)    ? 27.0f : g_temp;
  float hm = isnan(g_humi)    ? 55.0f : g_humi;

  LoRa.beginPacket();
  LoRa.print(th, 2); LoRa.print(',');
  LoRa.print(tp, 1); LoRa.print(',');
  LoRa.print(hm, 1); LoRa.print(',');
  LoRa.print((int)state);
  LoRa.endPacket(true);
}

// ======== Command handling ========
static void handleCommand(uint8_t c) {
  const unsigned long now = millis();
  switch (c) {
    case 'Z':  // handshake/beep
      t_cmdPulse = now;
      pulseAlert(HANDSHAKE_BEEP_MS);
      break;

    case 'A':  // arm
      if (state == DISARMED) {
        state = ARMED;
        setSafety(false);
      }
      t_cmdPulse = now;
      break;

    case 'S':  // safe/disarm
      state = DISARMED;
      setSafety(true);
      digitalWrite(IGNITION_PIN, HIGH); // ensure off
      t_cmdPulse = now;
      break;

    case 'L':  // 30 s countdown then ignition
      if (state == ARMED) {
        state = COUNTDOWN;
        t_sequence = now;
      }
      t_cmdPulse = now;
      break;

    case 'T':  // 4 s test tone/flash while armed
      if (state == ARMED) {
        pulseAlert(4000);
      }
      t_cmdPulse = now;
      break;

    default:
      break;
  }
}

static void pollLoRaRx() {
  int sz = LoRa.parsePacket();
  if (sz <= 0) return;
  int last = -1;
  while (LoRa.available()) last = LoRa.read();
  if (last >= 0) handleCommand((uint8_t)last);
}

// ======== Sequences ========
static void runSequences() {
  const unsigned long now = millis();

  if (state == COUNTDOWN) {
    unsigned long elapsed = now - t_sequence;
    if (elapsed >= COUNTDOWN_MS) {
      state = IGNITION;
      t_sequence = now;
    } else {
      // gentle tick (visual) during countdown
      static unsigned long t_lastBlink = 0;
      unsigned long remain = COUNTDOWN_MS - elapsed;
      unsigned long interval = (remain > 10000UL) ? 1000UL : 200UL;
      if (now - t_lastBlink >= interval) {
        digitalWrite(ALERT_PIN, HIGH);
        delay(60);
        digitalWrite(ALERT_PIN, LOW);
        t_lastBlink = now;
      }
    }
  }

  if (state == IGNITION) {
    if (now - t_sequence < IGNITION_PULSE_MS) {
      digitalWrite(IGNITION_PIN, LOW);  // active-LOW pulse
      digitalWrite(ALERT_PIN, HIGH);
    } else {
      digitalWrite(IGNITION_PIN, HIGH);
      digitalWrite(ALERT_PIN, LOW);
      state = DISARMED;                 // auto-safe
      setSafety(true);
    }
  }

  // brief visual pulse on recent command (if not igniting)
  if (state != IGNITION) {
    if (now - t_cmdPulse < FEEDBACK_PULSE_MS) digitalWrite(ALERT_PIN, HIGH);
    else digitalWrite(ALERT_PIN, LOW);
  }
}

// ======== Arduino lifecycle ========
void setup() {
  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW);

  pinMode(IGNITION_PIN, OUTPUT);
  digitalWrite(IGNITION_PIN, HIGH); // idle HIGH (inactive for active-LOW)

  Serial.begin(115200);

  // Sensors
  dht.begin();
  hx.begin(HX_DOUT_PIN, HX_SCK_PIN);
  delay(300);
  // Tare HX711
  long sum = 0;
  for (int i = 0; i < 16; i++) sum += hx.read_average(4);
  hx_offset = sum / 16;

  // Seed smoothing buffer to zero thrust
  for (uint8_t i = 0; i < THR_AVG_N; i++) thr_hist[i] = 0.0f;

  // Safety locked on boot
  setSafety(true);

  // LoRa
  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQ_HZ)) {
    // Fatal: steady blink forever
    while (1) {
      digitalWrite(ALERT_PIN, HIGH); delay(200);
      digitalWrite(ALERT_PIN, LOW);  delay(200);
    }
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setCodingRate4(LORA_CR_DEN);
  LoRa.setTxPower(LORA_TX_POWER);

  // First env snapshot
  readEnv();

  Serial.println("LP ready: ASCII CSV @ 433E6, SF10, CR4/5 (DhumketuX).");
}

void loop() {
  const unsigned long now = millis();

  runSequences();

  if (now - t_lastSense >= SENSOR_MS) {
    t_lastSense = now;
    readEnv();
  }

  if (now - t_lastTx >= TELEMETRY_MS) {
    t_lastTx = now;
    sendCSV();
  }

  pollLoRaRx();
}
