#include <SPI.h>
#include <LoRa.h>

#define NSS_PIN 10
#define RST_PIN 9
#define DIO0_PIN 2

String commandToSend = "";
float loadCellValue = 0.0;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Ground Station Initializing...");

  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  Serial.println("LoRa initialized successfully!");
  Serial.println("Type ARM, LAUNCH, or CHECK in Serial Monitor.");
}

void loop() {
  if (Serial.available()) {
    commandToSend = Serial.readStringUntil('\n');
    commandToSend.trim();
    if (commandToSend == "ARM" || commandToSend == "LAUNCH" || commandToSend == "CHECK") sendCommand(commandToSend);
    else Serial.println("Invalid command. Use: ARM, LAUNCH, or CHECK");
  }

  // Check for received packets
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String received = "";
    while (LoRa.available()) {
      received += (char)LoRa.read();
    }
    received.trim();
    Serial.print("Received raw: ");
    Serial.println(received);

    // Try to parse the response if it's in format "OK,<float>"
    if (received.startsWith("OK")) {
      int commaIndex = received.indexOf(',');
      if (commaIndex > 0 && commaIndex < received.length() - 1) {
        String valStr = received.substring(commaIndex + 1);
        loadCellValue = valStr.toFloat();

        Serial.println("✅ LaunchPad Connected!");
        Serial.print("📊 Load Cell Value: ");
        Serial.println(loadCellValue, 2);
      } 
      
      else {
        Serial.println("⚠️ OK received but no value found.");
      }
    } 
    
    else {
      Serial.print("📨 Other message: ");
      Serial.println(received);
    }
  }
}



void sendCommand(String cmd) {
  Serial.print("Sending command: ");
  Serial.println(cmd);
  LoRa.beginPacket();
  LoRa.print(cmd);
  LoRa.endPacket();
}
