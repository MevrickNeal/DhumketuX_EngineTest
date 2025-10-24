// --- Libraries ---
#include "HX711.h"
#include <SoftwareSerial.h>
#include <DHT.h>       
#include <Adafruit_Sensor.h> 

// --- Pin Definitions ---
// Load Cell (Thrust Measurement)
const int LOADCELL_DOUT_PIN = 3; 
const int LOADCELL_SCK_PIN = 2;  
const long CALIBRATION_FACTOR = 456000; // Use your calibrated factor!
HX711 scale;

// LoRa E32 (Communication) - Uses SoftwareSerial
const int LORA_RX_PIN = 10; 
const int LORA_TX_PIN = 11; 
const long LORA_BAUD = 9600; 
SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN); 

// DHT22 Sensor (Temperature/Humidity)
#define DHTPIN 6       
#define DHTTYPE DHT22  
DHT dht(DHTPIN, DHTTYPE);

// Control Modules
const int RELAY_PIN = 13; // *** UPDATED PIN TO D13 ***
const int SERVO_PIN = 5;  // Servo control pin

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
  
  // Setup Control Pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is OFF (safe state)
  // pinMode(SERVO_PIN, OUTPUT); // Servo library handles pin mode
}

void loop() {
  // --- 1. Read DHT22 Data ---
  currentHumi_RH = dht.readHumidity();
  currentTemp_C = dht.readTemperature(); 

  // --- 2. Read Thrust Data ---
  if (scale.is_ready()) {
    float mass_kg = scale.get_units(1); 
    currentThrust_N = mass_kg * 9.80665; 
    if (currentThrust_N < 0.1) {
      currentThrust_N = 0.0;
    }
  }

  // --- 3. Format & Transmit Data via LoRa ---
  // Format: "Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z"
  String payload = "Thrust:" + String(currentThrust_N, 2) + 
                   ",Temp:" + String(currentTemp_C, 1) + 
                   ",Humi:" + String(currentHumi_RH, 1);
  
  LoRaSerial.println(payload);
  
  // Debug Output
  Serial.print("TX: ");
  Serial.println(payload);
    
  // Update rate
  delay(100); 
}
