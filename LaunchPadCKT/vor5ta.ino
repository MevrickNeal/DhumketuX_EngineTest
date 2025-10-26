/**
 * DhumketuX LaunchPad (STM32 Bluepill) — ASCII CSV over LoRa
 * CSV line each ~100 ms: "thrustN,tempC,humidity%,state"
 * Commands from GS: Z(handshake beep), A(arm), S(safe), L(countdown), T(test tone)
 */

#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include <DHT.h>
#include <HX711.h>

// ---------------- Pin map (Bluepill) ----------------
#define LORA_CS_PIN       PA4
#define LORA_RST_PIN      PA3
#define LORA_DIO0_PIN     PA2

#define HX_DOUT_PIN       PB0
#define HX_SCK_PIN        PB1

#define DHT_PIN           PA8
#define DHT_TYPE          DHT22

#define SERVO_PIN         PA9
#define IGNITION_PIN      PC13      // Active-LOW relay
#define ALERT_PIN         PB10      // Buzzer or LED

// ---------------- LoRa radio ----------------
#define LORA_FREQ_HZ      433E6
#define LORA_SF           10        // matches GS
#define LORA_CR_DEN       5         // coding rate 4/5
// leave bandwidth/syncword at defaults so it matches GS defaults
#define LORA_TX_POWER     20        // 2..20 on RA-02 (PA_BOOST)

// ---------------- Timing ----------------
#define TELEMETRY_MS      100UL     // ~10 Hz
#define SENSOR_MS         1000UL    // DHT refresh
#define COUNTDOWN_MS      30000UL   // 30 s
#define IGNITION_PULSE_MS 300UL
#define FEEDBACK_PULSE_MS 50UL
#define HANDSHAKE_BEEP_MS 500UL

// ---------------- Servo positions ----------------
#define SERVO_LOCK_DEG    0
#define SERVO_ARM_DEG     90

// ---------------- State machine ----------------
enum State : uint8_t {
  DISARMED = 0,
  ARMED    = 1,
  COUNTDOWN= 2,
  IGNITION = 3
};

State state = DISARMED;

// ---------------- Globals ----------------
DHT dht(DHT_PIN, DHT_TYPE);
HX711 hx;
Servo safetyServo;

unsigned long t_lastTx = 0;
unsigned long t_lastSense = 0;
unsigned long t_sequence = 0;
unsigned long t_cmdPulse = 0;

float g_temp = NAN, g_humi = NAN;
float g_thrustN = 0.0f;

char line[80];   // CSV buffer

// HX711 calibration
// Set this so that (raw-offset)/SCALE = mass_kg
// Then thrust N = mass_kg * 9.81
long   hx_offset = 0;
float  hx_scale  = 100000.0f;   // placeholder; tune in field

// ---------------- Helpers ----------------
static void setSafety(bool locked) {
  if (!safetyServo.attached()) safetyServo.attach(SERVO_PIN);
  safetyServo.write(locked ? SERVO_LOCK_DEG : SERVO_ARM_DEG);
}

static void beep(unsigned ms, unsigned freq = 1000) {
  // tone() works on STM32 Arduino core on most pins; if not, LED blink still shows activity.
  #ifdef TONE_PIN_SUPPORT
  tone(ALERT_PIN, freq, ms);
  #else
  digitalWrite(ALERT_PIN, HIGH);
  delay(ms);
  digitalWrite(ALERT_PIN, LOW);
  #endif
}

static float readThrustN() {
  long raw = hx.read_average(8);
  long net = raw - hx_offset;
  float mass_kg = (float)net / hx_scale;
  return mass_kg * 9.81f;
}

static void readEnv() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) g_temp = t;
  if (!isnan(h)) g_humi = h;
}

static void sendCSV() {
  // state already in global "state"
  g_thrustN = readThrustN();
  // thrust(2dp), temp(1dp), humi(1dp), state
  int n = snprintf(line, sizeof(line), "%.2f,%.1f,%.1f,%d",
                   g_thrustN, g_temp, g_humi, (int)state);
  if (n < 0) return;

  LoRa.beginPacket();
  LoRa.write((const uint8_t*)line, (size_t)n);
  LoRa.endPacket(true); // async
}

// ---------------- Command handling ----------------
static void handleCommand(uint8_t c) {
  const unsigned long now = millis();
  switch (c) {
    case 'Z':  // handshake/beep
      t_cmdPulse = now;
      beep(HANDSHAKE_BEEP_MS, 1000);
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

    case 'L':  // 30s countdown then ignition
      if (state == ARMED) {
        state = COUNTDOWN;
        t_sequence = now;
      }
      t_cmdPulse = now;
      break;

    case 'T':  // short test tone while armed
      if (state == ARMED) {
        t_cmdPulse = now;
        beep(4000, 880);
      }
      break;

    default:
      break;
  }
}

static void pollLoRaRx() {
  int sz = LoRa.parsePacket();
  if (sz <= 0) return;

  // We expect single-byte commands; drain all, use the last byte
  int last = -1;
  while (LoRa.available()) last = LoRa.read();
  if (last >= 0) handleCommand((uint8_t)last);
}

// ---------------- Sequences ----------------
static void runSequences() {
  const unsigned long now = millis();

  // Countdown beeps: slow >10 s, fast <=10 s
  if (state == COUNTDOWN) {
    unsigned long elapsed = now - t_sequence;
    if (elapsed >= COUNTDOWN_MS) {
      // fire
      state = IGNITION;
      t_sequence = now;
      // fall through to ignition handling
    } else {
      unsigned long remain = COUNTDOWN_MS - elapsed;
      static unsigned long t_lastBeep = 0;
      unsigned long interval = (remain > 10000UL) ? 1000UL : 200UL;
      if (now - t_lastBeep >= interval) {
        beep(100, (remain > 10000UL) ? 600 : 1500);
        t_lastBeep = now;
      }
    }
  }

  if (state == IGNITION) {
    if (now - t_sequence < IGNITION_PULSE_MS) {
      // active LOW
      digitalWrite(IGNITION_PIN, LOW);
      digitalWrite(ALERT_PIN, HIGH);
    } else {
      // stop pulse, auto-safe
      digitalWrite(IGNITION_PIN, HIGH);
      digitalWrite(ALERT_PIN, LOW);
      state = DISARMED;
      setSafety(true);
    }
  }

  // brief visual pulse on recent command
  if (now - t_cmdPulse < FEEDBACK_PULSE_MS && state != IGNITION) {
    digitalWrite(ALERT_PIN, HIGH);
  } else if (state != IGNITION) {
    digitalWrite(ALERT_PIN, LOW);
  }
}

// ---------------- Arduino lifecycle ----------------
void setup() {
  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW);

  pinMode(IGNITION_PIN, OUTPUT);
  digitalWrite(IGNITION_PIN, HIGH);     // idle HIGH (inactive, active-LOW)

  Serial.begin(115200);

  // Sensors
  dht.begin();
  hx.begin(HX_DOUT_PIN, HX_SCK_PIN);
  delay(300);
  // Tare
  long sum = 0;
  for (int i=0;i<16;i++) sum += hx.read_average(4);
  hx_offset = sum / 16;

  // TODO: field-calibrate hx_scale:
  // place known mass M_kg → net_counts = (raw - offset)
  // set hx_scale = net_counts / M_kg;

  // Safety locked on boot
  setSafety(true);

  // LoRa
  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQ_HZ)) {
    // fatal: indicate via steady alert
    while (1) {
      digitalWrite(ALERT_PIN, HIGH);
      delay(200);
      digitalWrite(ALERT_PIN, LOW);
      delay(200);
    }
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setCodingRate4(LORA_CR_DEN);
  LoRa.setTxPower(LORA_TX_POWER);

  // Initial sensor snapshot
  readEnv();

  Serial.println("LP ready: ASCII CSV, waiting for commands (Z/A/S/L/T).");
}

void loop() {
  const unsigned long now = millis();

  // sequences (countdown/ignition + feedback)
  runSequences();

  // periodic environment read
  if (now - t_lastSense >= SENSOR_MS) {
    t_lastSense = now;
    readEnv();
  }

  // periodic CSV send
  if (now - t_lastTx >= TELEMETRY_MS) {
    t_lastTx = now;
    sendCSV();
  }

  // command RX
  pollLoRaRx();
}
