// DhumketuX Engine Test System - Ground Station Unit (GS)
// Role: Receives data via LoRa and relays it to the PC's Web Serial API.
// Required Libraries: SoftwareSerial, EByte

#include <SoftwareSerial.h>
#include "EByte.h" 

// --- LoRa E32 Pin Definitions ---
#define PIN_M0 8    
#define PIN_M1 9    
const int LORA_RX_PIN = 10; 
const int LORA_TX_PIN = 11; 
const long LORA_BAUD = 9600; 

SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN); 
EByteTransceiver Transceiver(LoRaSerial, PIN_M0, PIN_M1); 

void setup() {
  // 1. PC Communication (USB Serial) - MUST MATCH Web Serial config (9600)
  Serial.begin(9600); 
  
  // 2. Initialize E32 Transceiver and Set Mode
  LoRaSerial.begin(LORA_BAUD);
  Transceiver.init();
  
  // MUST MATCH Launch Pad settings! (ADR_2400 is 2.4 kbps)
  Transceiver.SetAirDataRate(ADR_2400); 
  Transceiver.SetMode(MODE_0_NORMAL); 

  Serial.println("Ground Station Ready (E32 Configured). Waiting for Launch Pad data...");
}

void loop() {
  // Check if LoRa has received any data
  // The logic here is simple: read everything from LoRa and pass it directly to PC Serial
  while (LoRaSerial.available()) {
    
    // Read the incoming data stream line by line
    String receivedData = LoRaSerial.readStringUntil('\n');

    if (receivedData.length() > 0) {
      // 3. Relay the received data immediately to the PC (Web Serial)
      // This line is read by the JavaScript UI:
      Serial.println(receivedData); 
    }
  }
}
