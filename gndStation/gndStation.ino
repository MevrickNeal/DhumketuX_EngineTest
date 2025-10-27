#include <SPI.h>
#include <LoRa.h>

#define NSS_PIN 10
#define RST_PIN 9
#define DIO0_PIN 2

enum lauchState {
  IDLE,
  AMRED,
  COUNTDOWN,
  LAUNCHED,
};

lauchState state = IDLE;


String commandToSend = "";
float loadCellValue = 0.0;

uint32_t timer = millis();

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;

  Serial.println("Ground Station Initializing...");

  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }

  Serial.println("LoRa initialized successfully!");
  Serial.println("Type ARM, LAUNCH, or CHECK in Serial Monitor.");
}

void loop() {
  if (Serial.available()) {
    commandToSend = Serial.readStringUntil('\n');
    commandToSend.trim();
    sendCommand(commandToSend);
  }

  incoming();

  if (state == IDLE) {
    if (millis() - timer > 5000) {
      sendCommand("CHECK");
      timer = millis();
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
