/**
 * @file GS_Receiver.ino
 * @brief DhumketuX Ground Station (GS) – Arduino Uno/Nano
 *
 * Matches LP packet:
 *   struct Telemetry_t { float thrust; float temperature; float humidity; int8_t isArmed; uint16_t checksum; } packed
 * Checksum = 16-bit sum of bytes with checksum field set to 0 while computing.
 *
 * Pins (per your table):
 *  LoRa CS   -> D10
 *  LoRa RST  -> D9
 *  LoRa DIO0 -> D2
 *  SPI SCK   -> D13 (HW)
 *  SPI MISO  -> D12 (HW)
 *  SPI MOSI  -> D11 (HW)
 *  Status LED-> D8  (Active HIGH pulse on RX/TX)
 *
 * Voltage note: RA-02 is 3.3 V only. Use level shifting for UNO outputs (SCK, MOSI, CS, DIO0).
 */

#include <SPI.h>
#include <LoRa.h>

// -------- Pins --------
#define LORA_CS_PIN     10
#define LORA_RST_PIN    9
#define LORA_DIO0_PIN   2
#define STATUS_LED_PIN  8   // per your latest pin map

// -------- Radio config (must match LP) --------
#define LORA_FREQ_HZ    433E6
#define LORA_BW         125E3
#define LORA_SF         7
#define LORA_CR_DEN     5        // 4/5
#define LORA_PREAMBLE   8
#define LORA_SYNCWORD   0x34

// -------- Timing --------
#define LED_PULSE_MS          50UL
#define CSV_BUF_SIZE          60

// -------- Packet --------
struct __attribute__((packed)) Telemetry_t {
  float thrust;
  float temperature;
  float humidity;
  int8_t isArmed;     // 0=Disarmed,1=Armed,2=Countdown,3=Ignition
  uint16_t checksum;  // 16-bit sum over bytes with this field zeroed during calc
};

char csvBuf[CSV_BUF_SIZE];
unsigned long lastActivity = 0;

// -------- Utils --------
uint16_t checksum16(const uint8_t* data, size_t len) {
  uint32_t sum = 0;
  for (size_t i = 0; i < len; i++) sum += data[i];
  return (uint16_t)(sum & 0xFFFF);
}

void pulseActivity() {
  lastActivity = millis();
}

void handleStatusLed() {
  if (millis() - lastActivity < LED_PULSE_MS) digitalWrite(STATUS_LED_PIN, HIGH);
  else digitalWrite(STATUS_LED_PIN, LOW);
}

void printCSV(const Telemetry_t& t, long rssi) {
  unsigned long t_ms = millis();
  // Format: thrust(N with 2dp), temp(C 1dp), humi(% 1dp), time_ms, isArmed
  int n = snprintf(csvBuf, CSV_BUF_SIZE, "%.2f,%.1f,%.1f,%lu,%d\r\n",
                   t.thrust, t.temperature, t.humidity, t_ms, t.isArmed);
  if (n > 0) Serial.write((const uint8_t*)csvBuf, (size_t)n);
  // Optional debug RSSI:
  // Serial.print(F("RSSI:")); Serial.println(rssi);
}

void sendLoRaCmd(uint8_t c) {
  LoRa.beginPacket();
  LoRa.write(c);
  LoRa.endPacket(true); // async
  pulseActivity();
  // Optional echo:
  Serial.print(F("TX Command: "));
  Serial.write(c);
  Serial.println();
}

// -------- Setup --------
void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  Serial.begin(115200);
  while (!Serial) {}

  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQ_HZ)) {
    Serial.println(F("LoRa init failed"));
    // leave LED off to indicate fault
  } else {
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR_DEN);
    LoRa.setPreambleLength(LORA_PREAMBLE);
    LoRa.setSyncWord(LORA_SYNCWORD);
    LoRa.setTxPower(14); // modest power for commands
    Serial.println(F("LoRa ready"));
  }
}

// -------- Loop --------
void loop() {
  handleStatusLed();

  // 1) Forward serial single-character commands to LP
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'A' || c == 'S' || c == 'T' || c == 'L' || c == 'I') {
      sendLoRaCmd((uint8_t)c);
    }
  }

  // 2) Receive telemetry packets
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    // Expect exact struct size; discard anything else
    if (packetSize == (int)sizeof(Telemetry_t)) {
      Telemetry_t pkt;
      uint8_t* p = (uint8_t*)&pkt;
      int i = 0;
      while (LoRa.available() && i < (int)sizeof(Telemetry_t)) {
        p[i++] = (uint8_t)LoRa.read();
      }
      if (i == (int)sizeof(Telemetry_t)) {
        // Verify checksum
        uint16_t rxChecksum = pkt.checksum;
        pkt.checksum = 0;
        uint16_t calc = checksum16((const uint8_t*)&pkt, sizeof(Telemetry_t));
        if (calc == rxChecksum) {
          printCSV(pkt, LoRa.packetRssi());
          pulseActivity();
        } else {
          Serial.print(F("CHK FAIL rx="));
          Serial.print(rxChecksum);
          Serial.print(F(" calc="));
          Serial.println(calc);
        }
      }
    } else {
      // Drain unexpected payload
      while (LoRa.available()) LoRa.read();
      Serial.print(F("Size mismatch: ")); Serial.println(packetSize);
    }
  }
}
