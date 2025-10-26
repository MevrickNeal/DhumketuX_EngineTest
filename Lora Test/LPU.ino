/**
 * @file LP_Bluepill.ino
 * @brief DhumketuX Launch Pad Unit (LP – STM32 Bluepill)
 *
 * MCU: STM32F103C8T6 (Bluepill) – Arduino Core for STM32 by ST
 * LoRa: RA-02 (SX1278) @ 433 MHz
 *
 * Wiring (matches your table):
 *  LoRa CS  -> PA4
 *  LoRa RST -> PB12
 *  LoRa DIO0-> PB13
 *  LoRa SCK -> PA5
 *  LoRa MISO-> PA6
 *  LoRa MOSI-> PA7
 *
 *  HX711 DOUT -> PB0
 *  HX711 SCK  -> PB1
 *  DHT22 DATA -> PA8
 *  Ignition Relay -> PC13 (Active-LOW, 300 ms pulse)
 *  Safety Servo   -> PA3  (PWM)
 *  Buzzer/LED     -> PB10 (HIGH = on)
 *
 * Telemetry_t binary packet (packed) → LoRa:
 *   float thrust, float temperature, float humidity, int8_t isArmed, uint16_t checksum
 * Checksum is 16-bit sum over bytes of the struct with checksum field zeroed.
 */

#include <SPI.h>
#include <LoRa.h>
#include <Servo.h>
#include <DHT.h>

// ---------------- Pins ----------------
#define LORA_CS_PIN     PA4
#define LORA_RST_PIN    PB12
#define LORA_DIO0_PIN   PB13

#define HX_DOUT_PIN     PB0
#define HX_SCK_PIN      PB1

#define DHT_PIN         PA8
#define DHT_TYPE        DHT22

#define IGNITION_PIN    PC13      // Active-LOW
#define SERVO_PIN       PA3
#define BUZZ_PIN        PB10

// ---------------- LoRa ----------------
#define LORA_FREQ_HZ    433E6
// Conservative link for reliability; must match GS:
#define LORA_BW         125E3
#define LORA_SF         7
#define LORA_CR_DEN     5         // 4/5
#define LORA_PREAMBLE   8
#define LORA_SYNCWORD   0x34

// ---------------- Timing ----------------
#define TELEMETRY_PERIOD_MS   50UL     // 20 Hz
#define DHT_PERIOD_MS         2000UL   // 0.5 Hz
#define HX_AVG_SAMPLES        8
#define IGNITION_PULSE_MS     300UL
#define COUNTDOWN_MS          30000UL  // 30 seconds

// ---------------- Servo angles ----------------
#define SERVO_DISARM_DEG  10
#define SERVO_ARM_DEG     90

// ---------------- States ----------------
enum ArmState : int8_t { DISARMED=0, ARMED=1, COUNTDOWN=2, IGNITION=3 };

// ---------------- Telemetry struct ----------------
struct __attribute__((packed)) Telemetry_t {
  float thrust;
  float temperature;
  float humidity;
  int8_t isArmed;   // 0,1,2,3
  uint16_t checksum;
};

// ---------------- Globals ----------------
DHT dht(DHT_PIN, DHT_TYPE);
Servo safetyServo;

volatile ArmState armState = DISARMED;
unsigned long t_lastTx = 0;
unsigned long t_lastDHT = 0;

float g_tempC = NAN;
float g_humi  = NAN;
long  g_hx_offset = 0;     // tare offset
float g_hx_scale  = 1.0f;  // counts-to-kg scaling, tune in field

// Countdown/ignition
bool countdownActive = false;
unsigned long countdownStart = 0;

bool ignitionPulse = false;
unsigned long ignitionStart = 0;

// ---------------- HX711 minimal driver (blocking bit-bang, short) ----------------
long hx711_readRaw() {
  // Wait for data ready (DOUT goes LOW)
  unsigned long t0 = millis();
  while (digitalRead(HX_DOUT_PIN) == HIGH) {
    if (millis() - t0 > 100) break; // prevent long stall
    // brief spin
  }

  // Read 24 bits, MSB first
  long value = 0;
  noInterrupts();
  for (int i = 0; i < 24; i++) {
    digitalWrite(HX_SCK_PIN, HIGH);
    // brief delay ~1-2 us
    __asm__ __volatile__("nop\nnop\nnop\nnop\n");
    value = (value << 1) | (digitalRead(HX_DOUT_PIN) ? 1 : 0);
    digitalWrite(HX_SCK_PIN, LOW);
    __asm__ __volatile__("nop\nnop\nnop\nnop\n");
  }
  // Set channel/gain to 128 (1 extra clock)
  digitalWrite(HX_SCK_PIN, HIGH);
  __asm__ __volatile__("nop\nnop\nnop\nnop\n");
  digitalWrite(HX_SCK_PIN, LOW);
  interrupts();

  // Convert from 24-bit two's complement
  if (value & 0x800000) value |= ~0xFFFFFFL;
  return value;
}

long hx711_readAverage(uint8_t times=HX_AVG_SAMPLES) {
  long sum = 0;
  for (uint8_t i=0; i<times; i++) sum += hx711_readRaw();
  return sum / (long)times;
}

float hx711_getThrust() {
  long raw = hx711_readAverage();
  long net = raw - g_hx_offset;
  // Convert counts to kg (tune g_hx_scale), then to Newtons if needed.
  // For now report "kg" as thrust proxy; multiply by 9.81 if you want N.
  float kg = (float)net / g_hx_scale;
  // You may prefer Newtons:
  float N = kg * 9.81f;
  return N; // Report Newtons as thrust
}

// ---------------- Utils ----------------
uint16_t checksum16(const uint8_t* data, size_t len) {
  uint32_t sum = 0;
  for (size_t i=0; i<len; i++) sum += data[i];
  return (uint16_t)(sum & 0xFFFF);
}

void sendTelemetry() {
  Telemetry_t pkt;
  pkt.thrust = hx711_getThrust();
  pkt.temperature = isnan(g_tempC) ? -1000.0f : g_tempC;
  pkt.humidity    = isnan(g_humi)  ? -1000.0f : g_humi;
  pkt.isArmed     = (int8_t)armState;
  pkt.checksum    = 0;

  // Compute checksum over struct with checksum field zeroed
  pkt.checksum = checksum16(reinterpret_cast<const uint8_t*>(&pkt), sizeof(Telemetry_t));

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(Telemetry_t));
  LoRa.endPacket(true); // async
}

void setArmState(ArmState s) {
  armState = s;
  if (s == ARMED) {
    safetyServo.write(SERVO_ARM_DEG);
    digitalWrite(BUZZ_PIN, HIGH); delay(50); digitalWrite(BUZZ_PIN, LOW);
  } else if (s == DISARMED) {
    safetyServo.write(SERVO_DISARM_DEG);
    digitalWrite(BUZZ_PIN, HIGH); delay(20); digitalWrite(BUZZ_PIN, LOW);
    delay(20); digitalWrite(BUZZ_PIN, HIGH); delay(20); digitalWrite(BUZZ_PIN, LOW);
  }
}

void startCountdown() {
  countdownActive = true;
  countdownStart = millis();
  setArmState(COUNTDOWN);
}

void abortCountdown() {
  countdownActive = false;
  setArmState(DISARMED);
}

void triggerIgnition() {
  // Only allow if armed or in countdown
  if (armState == ARMED || armState == COUNTDOWN) {
    armState = IGNITION;
    ignitionPulse = true;
    ignitionStart = millis();
    digitalWrite(IGNITION_PIN, LOW);   // active-LOW pulse starts
    digitalWrite(BUZZ_PIN, HIGH);
  }
}

// ---------------- Command handling ----------------
void handleLoRaCommand(uint8_t c) {
  switch (c) {
    case 'A': // Arm
      setArmState(ARMED);
      break;
    case 'S': // Safe/Disarm
      abortCountdown();
      setArmState(DISARMED);
      break;
    case 'T': // 30s countdown then ignite
      if (armState == ARMED) startCountdown();
      break;
    case 'L': // Immediate launch
    case 'I': // Ignition
      if (armState == ARMED || armState == COUNTDOWN) triggerIgnition();
      break;
    default:
      // ignore
      break;
  }
}

// ---------------- Setup ----------------
void setup() {
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(BUZZ_PIN, LOW);

  pinMode(IGNITION_PIN, OUTPUT);
  digitalWrite(IGNITION_PIN, HIGH); // idle HIGH (inactive for active-LOW relay)

  pinMode(HX_SCK_PIN, OUTPUT);
  pinMode(HX_DOUT_PIN, INPUT);

  safetyServo.attach(SERVO_PIN);
  safetyServo.write(SERVO_DISARM_DEG);

  dht.begin();

  // LoRa init
  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQ_HZ)) {
    // Buzz long if LoRa fails
    digitalWrite(BUZZ_PIN, HIGH); delay(500); digitalWrite(BUZZ_PIN, LOW);
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR_DEN);
  LoRa.setPreambleLength(LORA_PREAMBLE);
  LoRa.setSyncWord(LORA_SYNCWORD);
  LoRa.setTxPower(17); // 2-20 (uses PA_BOOST on RA-02)

  // Basic HX711 tare
  delay(300);
  g_hx_offset = hx711_readAverage(16);
  // Set a provisional scale. Calibrate in field:
  // Place known mass M_kg, measure net counts = (raw - offset), set g_hx_scale = net / M_kg
  g_hx_scale = 100000.0f; // placeholder; tune this

  setArmState(DISARMED);
  t_lastTx  = millis();
  t_lastDHT = millis();
}

// ---------------- Loop ----------------
void loop() {
  unsigned long now = millis();

  // Poll DHT at low rate
  if (now - t_lastDHT >= DHT_PERIOD_MS) {
    t_lastDHT = now;
    float t = dht.readTemperature(); // °C
    float h = dht.readHumidity();
    if (!isnan(t)) g_tempC = t;
    if (!isnan(h)) g_humi  = h;
  }

  // Handle countdown
  if (countdownActive) {
    unsigned long elapsed = now - countdownStart;
    if (elapsed >= COUNTDOWN_MS) {
      countdownActive = false;
      triggerIgnition();
    }
  }

  // Handle ignition pulse non-blocking
  if (ignitionPulse) {
    if (now - ignitionStart >= IGNITION_PULSE_MS) {
      ignitionPulse = false;
      digitalWrite(IGNITION_PIN, HIGH); // end pulse
      digitalWrite(BUZZ_PIN, LOW);
      // After ignition, remain in IGNITION state for GS to log; user can send 'S' to safe.
    }
  }

  // Receive LoRa commands (single-byte)
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    while (LoRa.available()) {
      uint8_t cmd = (uint8_t)LoRa.read();
      handleLoRaCommand(cmd);
    }
  }

  // Periodic telemetry TX
  if (now - t_lastTx >= TELEMETRY_PERIOD_MS) {
    t_lastTx = now;
    sendTelemetry();
  }
}
