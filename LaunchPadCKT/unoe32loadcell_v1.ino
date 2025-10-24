// --- Libraries ---
#include "HX711.h"
#include <SoftwareSerial.h>
#include <DHT.h>       // Include the DHT library
#include <Adafruit_Sensor.h> // Required by DHT library

// --- Pin Definitions ---
// Load Cell (Thrust Measurement)
const int LOADCELL_DOUT_PIN = 3; 
const int LOADCELL_SCK_PIN = 2;  
const long CALIBRATION_FACTOR = 456000; 
HX711 scale;

// LoRa E32 (Communication)
const int LORA_RX_PIN = 10; 
const int LORA_TX_PIN = 11; 
const long LORA_BAUD = 9600; 
SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN); 

// DHT22 Sensor (Temperature/Humidity)
#define DHTPIN 6       // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22  // DHT 22 (AM2302)
DHT dht(DHTPIN, DHTTYPE);

// --- Variables ---
float currentThrust_N = 0.0; 
float currentTemp_C = 0.0;
float currentHumi_RH = 0.0;

void setup() {
  Serial.begin(9600);
  LoRaSerial.begin(LORA_BAUD);
  
  // Initialize Load Cell and Tare
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare(); 
  
  // Initialize DHT Sensor
  dht.begin(); 
  
  // (Control pins setup omitted for brevity, but should be here)
}

void loop() {
  // --- 1. Read DHT22 Data (requires a 2-second interval, but we'll read often for continuous updates) ---
  currentHumi_RH = dht.readHumidity();
  currentTemp_C = dht.readTemperature(); // Reads in Celsius by default

  // Check if any reads failed and exit early (DHT sensors can be slow)
  if (isnan(currentHumi_RH) || isnan(currentTemp_C)) {
    Serial.println("Warning: Failed to read from DHT sensor!");
    // Keep using the previous valid data if available, or send zero.
  }

  // --- 2. Read Thrust Data ---
  if (scale.is_ready()) {
    float mass_kg = scale.get_units(1); 
    currentThrust_N = mass_kg * 9.80665; 
    if (currentThrust_N < 0.1) {
      currentThrust_N = 0.0;
    }
  }

  // --- 3. Format Data for Transmission via LoRa ---
  // Combine all measurements into a single, comma-separated payload for easier parsing
  // Format: "Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z"
  String payload = "Thrust:" + String(currentThrust_N, 2) + 
                   ",Temp:" + String(currentTemp_C, 1) + 
                   ",Humi:" + String(currentHumi_RH, 1);
  
  // --- 4. Transmit Data ---
  LoRaSerial.println(payload);
  
  // --- 5. Debug Output ---
  Serial.print("TX: ");
  Serial.println(payload);
    
  // Delay defines the update rate (10 times per second)
  delay(100); 
}
