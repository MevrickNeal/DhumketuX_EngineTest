// --- Libraries ---
#include "HX711.h"
#include <SoftwareSerial.h>

// --- Pin Definitions ---
// Load Cell (Thrust Measurement)
const int LOADCELL_DOUT_PIN = 3; 
const int LOADCELL_SCK_PIN = 2;  
const long CALIBRATION_FACTOR = 456000; // Use your calibrated factor!
HX711 scale;

// LoRa E32 (Communication) - Uses SoftwareSerial
const int LORA_RX_PIN = 10; // Connects to E32's TX
const int LORA_TX_PIN = 11; // Connects to E32's RX
const long LORA_BAUD = 9600; 
SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN); // RX, TX

// Control Modules
const int RELAY_PIN = 13; // Ignition Control
// Note: We'll add the Servo library and control later if needed.

// --- Variables ---
float currentThrust_N = 0.0; 

void setup() {
  // 1. Setup Serial for Debugging (PC connection)
  Serial.begin(9600);
  
  // 2. Setup LoRa Communication
  LoRaSerial.begin(LORA_BAUD);
  
  // 3. Setup Load Cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(CALIBRATION_FACTOR);
  
  // Tare the scale (set current load as zero)
  Serial.println("Taring the scale... Done.");
  scale.tare(); 
  
  // 4. Setup Control Pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is OFF (safe state)
}

void loop() {
  // --- 1. Read Thrust Data ---
  if (scale.is_ready()) {
    // Read the mass and convert it to Newtons
    float mass_kg = scale.get_units(1); // Read a single, quick unit measurement
    currentThrust_N = mass_kg * 9.80665; 
    
    // Safety: ensure thrust is not negative unless actively pulling
    if (currentThrust_N < 0.1) {
      currentThrust_N = 0.0;
    }
    
    // --- 2. Format Data for Transmission ---
    // The Ground Station expects a specific format to parse easily.
    // Format: "Thrust: XX.XX N"
    String payload = "Thrust: " + String(currentThrust_N, 2) + " N";
    
    // --- 3. Transmit Data via LoRa ---
    LoRaSerial.println(payload);
    
    // --- 4. Debug Output ---
    Serial.print("TX: ");
    Serial.println(payload);
    
  } else {
    Serial.println("Warning: HX711 not ready.");
  }

  // --- 5. Add Control Logic Here (Relay/Servo) ---
  // In a final design, you would check for commands received from the GS here
  // Example: if (LoRaSerial.available()) { readCommand(); }

  // Delay defines the update rate (10 times per second)
  delay(100); 
}
