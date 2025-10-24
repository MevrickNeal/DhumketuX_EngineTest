#include "HX711.h"
#include <SoftwareSerial.h>
#include <DHT.h>       
#include <Adafruit_Sensor.h> 
#include <Servo.h>     // Include the Servo Library for safety control

// --- PIN DEFINITIONS ---
// Load Cell
const int LOADCELL_DOUT_PIN = 3; 
const int LOADCELL_SCK_PIN = 2;  
const long CALIBRATION_FACTOR = 456000; 
HX711 scale;

// LoRa E32 (Communication)
const int LORA_RX_PIN = 10; 
const int LORA_TX_PIN = 11; 
const long LORA_BAUD = 9600; 
SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN); 

// DHT22 Sensor
#define DHTPIN 6       
#define DHTTYPE DHT22  
DHT dht(DHTPIN, DHTTYPE);

// Control Modules
const int RELAY_PIN = 13; // Ignition Relay (D13)
const int SERVO_PIN = 5;  // Safety Lever Servo (D5)
Servo safetyServo;

// --- STATE AND VARIABLES ---
float currentThrust_N = 0.0; 
float currentTemp_C = 0.0;
float currentHumi_RH = 0.0;
bool isArmed = false;

// --- FUNCTION PROTOTYPES ---
void executeCommand(char cmd);
void setSafePosition();
void setArmedPosition();

void setup() {
  Serial.begin(9600);
  LoRaSerial.begin(LORA_BAUD);
  
  // 1. Initialize Control Modules
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 
  safetyServo.attach(SERVO_PIN);
  
  // Set to SAFE position by default
  setSafePosition();
  
  // 2. Initialize Sensors
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare(); 
  dht.begin(); 
  Serial.println("Launch Pad Ready.");
}

void loop() {
  // --- 1. HANDLE INCOMING COMMANDS ---
  if (LoRaSerial.available()) {
    char command = LoRaSerial.read();
    executeCommand(command);
  }

  // --- 2. READ & TRANSMIT TELEMETRY (Every 100ms) ---
  readSensors();
  transmitTelemetry();

  delay(100); 
}

// -------------------------------------------------------------------
// --- COMMAND EXECUTION FUNCTIONS ---
// -------------------------------------------------------------------

void executeCommand(char cmd) {
  Serial.print("Command Received: ");
  Serial.println(cmd);

  switch (cmd) {
    case 'A': // ARM SYSTEM
      setArmedPosition();
      isArmed = true;
      Serial.println("System ARMED.");
      break;
      
    case 'S': // SAFE/DISARM SYSTEM
      setSafePosition();
      isArmed = false;
      Serial.println("System DISARMED.");
      break;

    case 'T': // TEST FIRE (brief pulse)
      if (isArmed) {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("TEST FIRE: IGNITION ON (50ms)");
        delay(50); // Brief pulse for testing
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("TEST FIRE: IGNITION OFF");
      } else {
        Serial.println("IGNITION BLOCKED: Not ARMED.");
      }
      break;

    case 'I': // LAUNCH/FULL FIRE
      if (isArmed) {
        Serial.println("!!! LAUNCH SEQUENCE INITIATED !!!");
        // In a real launch, the relay stays HIGH until burn-out, 
        // but here we just show the action.
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("IGNITION ON.");
        // Note: Actual launch control would involve a timer/sensor
      } else {
        Serial.println("IGNITION BLOCKED: Not ARMED.");
      }
      break;
  }
}

void setSafePosition() {
  // Servo position for safety lock (adjust based on your mechanism)
  safetyServo.write(10); 
  Serial.println("Safety Lever: SAFE (10 deg).");
}

void setArmedPosition() {
  // Servo position for arming (safety lock disengaged)
  safetyServo.write(170); 
  Serial.println("Safety Lever: ARMED (170 deg).");
}

// -------------------------------------------------------------------
// --- TELEMETRY FUNCTIONS ---
// -------------------------------------------------------------------

void readSensors() {
  // Read DHT22
  currentHumi_RH = dht.readHumidity();
  currentTemp_C = dht.readTemperature(); 

  // Read Load Cell
  if (scale.is_ready()) {
    float mass_kg = scale.get_units(1); 
    currentThrust_N = mass_kg * 9.80665; 
    if (currentThrust_N < 0.1) {
      currentThrust_N = 0.0;
    }
  }
}

void transmitTelemetry() {
  // Format: "Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z"
  String payload = "Thrust:" + String(currentThrust_N, 2) + 
                   ",Temp:" + String(currentTemp_C, 1) + 
                   ",Humi:" + String(currentHumi_RH, 1);
  
  LoRaSerial.println(payload);
  
  // Debug Output
  Serial.print("TX: ");
  Serial.println(payload);
}
