/**
 * @file LPU_DHT_Test.ino
 * @brief LPU BARE-METAL TEST: Transmit Load Cell + DHT data via LoRa.
 *
 * This test verifies the core LoRa link with all sensors.
 * It transmits a simple string: "Thrust:XX.XX,T:YY.Y,H:ZZ.Z"
 */

#include <SPI.h>
#include <LoRa.h> // Standard LoRa library
#include <DHT.h>
#include <HX711.h> // Using HX711 by Rob Tillaart
#include <Wire.h> 

// --- Configuration ---
#define LORA_FREQ_MHZ       433E6       
#define LORA_CS_PIN         PA4
#define LORA_RST_PIN        PA3 // Your working FC pinout
#define LORA_DIO0_PIN       PA2

#define DHT_PIN             PA8
#define DHT_TYPE            DHT22

// --- NEW: Load Cell Config ---
#define LOADCELL_DOUT_PIN   PB0
#define LOADCELL_CLK_PIN    PB1
#define CALIBRATION_FACTOR  -400.0f // CRITICAL: Must be calibrated by the user

// --- Global Objects ---
DHT dht(DHT_PIN, DHT_TYPE);
HX711 LoadCell;
char txString[50]; // Increased buffer for new string

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- LPU Load Cell + DHT Test ---");

    // 1. Initialize DHT
    dht.begin();
    
    // 2. --- NEW: Initialize Load Cell ---
    Serial.print("Initializing Load Cell (HX711)...");
    LoadCell.begin(LOADCELL_DOUT_PIN, LOADCELL_CLK_PIN);
    LoadCell.set_scale(CALIBRATION_FACTOR); 
    LoadCell.tare(); // Zero the scale at boot
    Serial.println("OK (Tared).");
    
    // 3. Initialize LoRa
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(LORA_FREQ_MHZ)) { 
        Serial.println("FATAL: LoRa Init Failed!");
        while (1);
    }

    // *** CRITICAL: Explicitly set LoRa parameters ***
    LoRa.setSpreadingFactor(10);
    LoRa.setCodingRate4(5);
    LoRa.setTxPower(20);

    Serial.println("LoRa TX Initialized. Starting transmission...");
}

void loop() {
    // 1. Read Sensors
    float temp = dht.readTemperature();
    float humi = dht.readHumidity();
    float thrust = LoadCell.get_units(1); // Read 1 sample

    if (isnan(temp) || isnan(humi)) {
        Serial.println("DHT Read Failure");
        // Don't stop, just send bad data
        temp = -99.9;
        humi = -99.9;
    }

    // 2. Format the new string
    snprintf(txString, sizeof(txString), "Thrust:%.2f,T:%.1f,H:%.1f", thrust, temp, humi);

    // 3. Transmit the packet
    LoRa.beginPacket();
    LoRa.print(txString);
    LoRa.endPacket(true); // Blocking send

    // 4. Print to local LPU serial monitor for confirmation
    Serial.print("TX Packet: [");
    Serial.print(txString);
    Serial.println("]");
    
    delay(1000); // Send one packet per second
}

