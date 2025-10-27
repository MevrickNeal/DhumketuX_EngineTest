#include <SPI.h>
#include <LoRa.h>

#define NSS_PIN  PA4
#define RST_PIN  PB12
#define DIO0_PIN PB13

float loadCellValue = 12.75; // Dummy value for now

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("LaunchPad Initializing...");

  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  Serial.println("LoRa initialized successfully on STM32!");
}

void loop() {
  // Check for incoming packets
  
}

void sendStatus() {
  // Example response: "OK,12.75"
  Serial.println("Responding with OK and load cell value...");
  LoRa.beginPacket();
  LoRa.print("OK,");
  LoRa.print(loadCellValue, 1); // send 2 decimal precision
  LoRa.endPacket();
}
