// DhumketuX Engine Test System - Launch Pad Unit (LP)
// Role: Reads sensors (Thrust, Temp/Humi), controls Relay/Servo, and transmits data via LoRa.
// Required Libraries: HX711, SoftwareSerial, DHT, Adafruit_Sensor, Servo, EByte

#include "HX711.h"
#include <SoftwareSerial.h>
#include <DHT.h>       
#include <Adafruit_Sensor.h> 
#include <Servo.h>     
#include "EByte.h" 

// --- PIN DEFINITIONS ---
// LoRa Control Pins (D8, D9 for E32 Mode Selection)
#define PIN_M0 8    
#define PIN_M1 9    
const int LORA_RX_PIN = 10; // Connects to E32 TX
const int LORA_TX_PIN = 11; // Connects to E32 RX
const long LORA_BAUD = 9600; 

SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN); 
EByteTransceiver Transceiver(LoRaSerial, PIN_M0, PIN_M1); 

// Load Cell (HX711)
const int LOADCELL_DOUT_PIN = 3; 
const int LOADCELL_SCK_PIN = 2;  
const long CALIBRATION_FACTOR = 456000; // <<< UPDATE THIS WITH YOUR CALIBRATION VALUE!
HX711 scale;

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
unsigned long lastTransmitTime = 0;
const long transmitInterval = 100; // 100ms update rate

// --- FUNCTION PROTOTYPES ---
void executeCommand(char cmd);
void setSafePosition();
void setArmedPosition();
void readSensors();
void transmitTelemetry();

void setup() {
  Serial.begin(9600); // PC Debugging Serial
  
  // 1. Initialize E32 Transceiver and Set Mode
  LoRaSerial.begin(LORA_BAUD);
  Transceiver.init();
  
  // Set consistent, reliable air data rate for both modules (2.4 kbps is robust)
  Transceiver.SetAirDataRate(ADR_2400); 
  // Set Transmit Power (using a medium setting like 50mW)
  Transceiver.SetTransmitPower(OPT_TP17); 
  // Set the E32 to Normal Mode (Mode 0) for communication
  Transceiver.SetMode(MODE_0_NORMAL); 

  // 2. Initialize Controls
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 
  safetyServo.attach(SERVO_PIN);
  setSafePosition();
  
  // 3. Initialize Sensors
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare(); 
  dht.begin(); 
  
  Serial.println("Launch Pad Ready (E32 Configured).");
}

void loop() {
  // --- 1. HANDLE INCOMING COMMANDS (from Ground Station) ---
  if (LoRaSerial.available()) {
    char command = LoRaSerial.read();
    executeCommand(command);
  }

  // --- 2. READ & TRANSMIT TELEMETRY (rate controlled) ---
  if (millis() - lastTransmitTime >= transmitInterval) {
      readSensors();
      transmitTelemetry();
      lastTransmitTime = millis();
  }
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
      digitalWrite(RELAY_PIN, LOW); // Force relay off
      Serial.println("System DISARMED.");
      break;

    case 'T': // TEST FIRE (300ms pulse)
      if (isArmed) {
        Serial.println("TEST FIRE: IGNITION ON (300ms)");
        digitalWrite(RELAY_PIN, HIGH);
        delay(300); 
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("TEST FIRE: IGNITION OFF");
      } else {
        Serial.println("IGNITION BLOCKED: Not ARMED.");
      }
      break;

    case 'I': // LAUNCH/FULL FIRE
      if (isArmed) {
        Serial.println("!!! LAUNCH SEQUENCE INITIATED: IGNITION ON !!!");
        digitalWrite(RELAY_PIN, HIGH);
        // Note: Relay stays HIGH until system is manually disarmed or shutdown.
      } else {
        Serial.println("IGNITION BLOCKED: Not ARMED.");
      }
      break;
  }
}

void setSafePosition() {
  // Adjust angle as needed for your physical mechanism
  safetyServo.write(10); 
  Serial.println("Safety Lever: SAFE (10 deg).");
}

void setArmedPosition() {
  // Adjust angle as needed for your physical mechanism
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
    // Filter noise floor
    if (currentThrust_N < 0.1 && currentThrust_N > -0.1) {
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
