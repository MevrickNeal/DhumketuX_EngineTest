// --- Libraries ---
#include <SoftwareSerial.h>

// --- LoRa E32 Pin Definitions ---
// Connect the E32's TX and RX pins to these SoftwareSerial pins
const int LORA_RX_PIN = 10; // Connects to E32's TX
const int LORA_TX_PIN = 11; // Connects to E32's RX
const long LORA_BAUD = 9600; // E32 modules often default to 9600

SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN); // RX, TX

void setup() {
  // 1. PC Communication (via USB to Web Serial API)
  Serial.begin(9600); 
  Serial.println("Ground Station Ready. Waiting for Launch Pad data...");

  // 2. LoRa Module Communication
  LoRaSerial.begin(LORA_BAUD);
}

void loop() {
  // Check if LoRa has received any data
  if (LoRaSerial.available()) {
    
    // Read the incoming data stream from the LoRa module
    String receivedData = LoRaSerial.readStringUntil('\n');

    if (receivedData.length() > 0) {
      // 3. Relay the received data immediately to the PC (Web Serial)
      // This is the crucial line: it sends the data to your browser UI
      Serial.println(receivedData); 
      
      // Print locally for debugging
      Serial.print("Received (PC): ");
      Serial.println(receivedData);
    }
  }
}
