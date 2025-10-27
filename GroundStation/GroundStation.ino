/**
 * DhumketuX Ground Station (GS) — ASCII CSV bridge
 * Works with LP that sends: "thrust,temp,hum,state"
 *
 * Uno/Nano pins (match your table):
 *  LoRa CS   -> D10
 *  LoRa RST  -> D9
 *  LoRa DIO0 -> D2
 *  SCK/MISO/MOSI -> D13/D12/D11 (HW SPI)
 *  Status LED-> D8 (HIGH = on)
 *
 * Notes:
 *  - RA-02 is 3.3 V ONLY. Level-shift UNO outputs: SCK, MOSI, CS, DIO0.
 *  - LP uses ASCII CSV and LoRa defaults except SF=10, CR=4/5, Freq=433E6.
 *  - We mirror those here; leave bandwidth/syncword at library defaults.
 */

#include <SPI.h>
#include <LoRa.h>

// -------- Pin map --------
#define LORA_CS_PIN     10
#define LORA_RST_PIN    9
#define LORA_DIO0_PIN   2
#define STATUS_LED_PIN  8

// -------- Radio params (mirror LP) --------
#define LORA_FREQ_HZ    433E6
#define LORA_SF         10      // LP sets 10
#define LORA_CR_DEN     5       // coding rate 4/5
// Leave BW and SyncWord at library defaults to match LP defaults

// -------- Timing --------
#define LED_PULSE_MS    50UL

// -------- State --------
unsigned long lastActivity = 0;

static void pulseActivity() { lastActivity = millis(); }
static void handleStatusLed() {
  if (millis() - lastActivity < LED_PULSE_MS) digitalWrite(STATUS_LED_PIN, HIGH);
  else digitalWrite(STATUS_LED_PIN, LOW);
}

// Send a single-byte command to LP
static void sendLoRaCmd(uint8_t c) {
  LoRa.beginPacket();
  LoRa.write(c);
  LoRa.endPacket(true);   // async
  pulseActivity();
  // Optional local echo
  Serial.print(F("TX Command: ")); Serial.write(c); Serial.println();
}

// Forward ASCII CSV packets from LP to USB serial
static void checkLoRaTelemetry() {
  int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) return;

  // Read entire packet as text
  String line;
  while (LoRa.available()) {
    char ch = (char)LoRa.read();
    line += ch;
  }
  // Trim and forward as a single CSV line (ensure newline)
  line.trim();
  if (line.length()) {
    Serial.println(line); // e.g. "123.45,28.1,60.2,1"
    pulseActivity();
  }
}

// Read keyboard and forward commands to LP
static void checkUsbCommands() {
  if (!Serial.available()) return;
  char c = Serial.read();
  // Allowed: Z (handshake), A (arm), S (safe), L (countdown), T (test)
  if (c=='Z' || c=='A' || c=='S' || c=='L' || c=='T') {
    sendLoRaCmd((uint8_t)c);
  }
}

void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  Serial.begin(115200);
  while (!Serial) {}

  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQ_HZ)) {
    Serial.println(F("LoRa init failed"));
    return;
  }
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setCodingRate4(LORA_CR_DEN);
  // Do NOT set bandwidth or syncword here so we stay on library defaults like LP

  Serial.println(F("GS ready (ASCII mode). Sending handshake..."));
  delay(100);
  sendLoRaCmd('Z');  // one-time beep/handshake on LP
}

void loop() {
  handleStatusLed();
  checkUsbCommands();
  checkLoRaTelemetry();
}
