// --- Libraries ---
#include "HX711.h"
#include <SoftwareSerial.h>
#include <DHT.h>       
#include <Adafruit_Sensor.h> 
#include <Servo.h>     
#include "LoRa_E32.h" // <<< UPDATED: Use the correct header file you found

// --- PIN DEFINITIONS ---
// LoRa Control Pins
#define PIN_M0 8    
#define PIN_M1 9    
const int LORA_RX_PIN = 10; 
const int LORA_TX_PIN = 11; 
const long LORA_BAUD = 9600; 

SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN); 
// We must assume the class name used in the library is 'LoRa_E32' based on the header file name.
LoRa_E32 Transceiver(&LoRaSerial, PIN_M0, PIN_M1); // <<< UPDATED: Using the SoftwareSerial pointer constructor

// Load Cell, DHT22, Control Pins (Unchanged)
const int LOADCELL_DOUT_PIN = 3; 
const int LOADCELL_SCK_PIN = 2;  
const long CALIBRATION_FACTOR = 456000; 
HX711 scale;
#define DHTPIN 6       
#define DHTTYPE DHT22  
DHT dht(DHTPIN, DHTTYPE);
const int RELAY_PIN = 13; 
const int SERVO_PIN = 5;  
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
void readSensors();
void transmitTelemetry();


void setup() {
  Serial.begin(9600);
  
  // 1. Initialize E32 Transceiver and Set Mode
  LoRaSerial.begin(LORA_BAUD);
  
  // NOTE: Initialization method may differ slightly. We use the most common one.
  ResponseStatus rs = Transceiver.begin(); 
  Serial.print("LoRa Initialization Status: ");
  Serial.println(rs.getResponseDescription());
  
  // Set consistent, reliable air data rate for both modules (2.4 kbps)
  Transceiver.setAirDataRate(ADR_2400); 
  
  // Set Transmit Power 
  Transceiver.setTransmissionPower(OPT_TP17); 
  
  // Set the E32 to Normal Mode (Mode 0) for communication
  Transceiver.setMode(MODE_0_NORMAL); 

  // 2. Initialize Controls (D13 Relay, D5 Servo)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 
  safetyServo.attach(SERVO_PIN);
  setSafePosition();
  
  // 3. Initialize Sensors (Load Cell, DHT22)
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare(); 
  dht.begin(); 
  
  Serial.println("Launch Pad Ready (LoRa Configured).");
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
// --- COMMAND EXECUTION FUNCTIONS (SAME AS BEFORE) ---
// -------------------------------------------------------------------

void executeCommand(char cmd) {
  Serial.print("Command Received: ");
  Serial.println(cmd);

  switch (cmd) {
    case 'A': 
      setArmedPosition();
      isArmed = true;
      Serial.println("System ARMED.");
      break;
      
    case 'S': 
      setSafePosition();
      isArmed = false;
      Serial.println("System DISARMED.");
      break;

    case 'T': 
      if (isArmed) {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("TEST FIRE: IGNITION ON (300ms)");
        delay(300); // Test Fire Pulse
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("TEST FIRE: IGNITION OFF");
      } else {
        Serial.println("IGNITION BLOCKED: Not ARMED.");
      }
      break;

    case 'I': 
      if (isArmed) {
        Serial.println("!!! LAUNCH SEQUENCE INITIATED !!!");
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("IGNITION ON.");
      } else {
        Serial.println("IGNITION BLOCKED: Not ARMED.");
      }
      break;
  }
}

void setSafePosition() {
  safetyServo.write(10); 
  Serial.println("Safety Lever: SAFE (10 deg).");
}

void setArmedPosition() {
  safetyServo.write(170); 
  Serial.println("Safety Lever: ARMED (170 deg).");
}

// -------------------------------------------------------------------
// --- TELEMETRY FUNCTIONS (SAME AS BEFORE) ---
// -------------------------------------------------------------------

void readSensors() {
  currentHumi_RH = dht.readHumidity();
  currentTemp_C = dht.readTemperature(); 

  if (scale.is_ready()) {
    float mass_kg = scale.get_units(1); 
    currentThrust_N = mass_kg * 9.80665; 
    if (currentThrust_N < 0.1) {
      currentThrust_N = 0.0;
    }
  }
}

void transmitTelemetry() {
  String payload = "Thrust:" + String(currentThrust_N, 2) + 
                   ",Temp:" + String(currentTemp_C, 1) + 
                   ",Humi:" + String(currentHumi_RH, 1);
  
  // We use the underlying SoftwareSerial object for simple transparent transmission
  LoRaSerial.println(payload);
}
