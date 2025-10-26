 * @file GS_DHT_Test.ino
 * @brief GS BARE-METAL TEST: Receive ONLY Load Cell + DHT data.
 *
 * This test verifies the core LoRa link.
 * It prints any received string to Serial and blinks D7 LED.
 * This code is the receiver for the LPU_DHT_Test.ino sketch.
 */

#include <SPI.h>
#include <LoRa.h> // Standard LoRa library
#include <Wire.h>

// --- Configuration ---
#define LORA_FREQ_MHZ       433E6 
#define LORA_CS_PIN         10
#define LORA_RST_PIN        9
#define LORA_DIO0_PIN       2
#define STATUS_LED_PIN      7

char rxBuffer[50]; // Buffer for received string

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- GS Load Cell + DHT Test ---");
    
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    
    // 1. Initialize LoRa
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(LORA_FREQ_MHZ)) { 
        Serial.println("FATAL: LoRa Init Failed!");
        while (1);
    }

    // *** CRITICAL: Match LPU's parameters ***
    LoRa.setSpreadingFactor(10);
    LoRa.setCodingRate4(5);
    // LoRa.setTxPower(20); // Not required for RX, but good practice to note

    Serial.println("LoRa RX Initialized. Listening for packets...");
    LoRa.receive(); // Enter continuous receive mode
}

void loop() {
    int packetSize = LoRa.parsePacket();
    
    if (packetSize) {
        // --- Packet Received! ---
        
        // 1. Blink LED
        digitalWrite(STATUS_LED_PIN, HIGH);
        
        // 2. Read the string
        int i = 0;
        while (LoRa.available() && i < sizeof(rxBuffer) - 1) {
            rxBuffer[i++] = (char)LoRa.read();
        }
        rxBuffer[i] = '\0'; // Null-terminate
        
        // 3. Print to local GS serial monitor
        Serial.print("RX Packet: [");
        Serial.print(rxBuffer);
        Serial.print("] | RSSI: ");
        Serial.println(LoRa.packetRssi());
        
        // 4. Turn off LED
        delay(50); // Keep LED on for 50ms
        digitalWrite(STATUS_LED_PIN, LOW);
        
        // Go back to listening
        LoRa.receive(); 
    }
}
