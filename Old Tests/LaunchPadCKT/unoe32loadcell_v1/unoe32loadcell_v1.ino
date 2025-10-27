// DhumketuX Engine Test System - Launch Pad Unit (LP)
// Role: Reads sensors, executes commands, and transmits FIXED structured telemetry.
// Required Libraries: HX711, SoftwareSerial, DHT, Adafruit_Sensor, Servo, LoRa_E32 (Renzo Mischianti)

#include "HX711.h"
#include <SoftwareSerial.h>
#include <DHT.h>       
#include <Adafruit_Sensor.h> 
#include <Servo.h>     
#include "LoRa_E32.h" 

// --- PIN DEFINITIONS ---
// LoRa Control Pins
#define PIN_M0 8    
#define PIN_M1 9    
#define PIN_AUX 7 // AUX pin is defined for constructor compatibility
const int LORA_RX_PIN = 10; 
const int LORA_TX_PIN = 11; 
const long LORA_BAUD = 9600; 

SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN); 

// FIX: Constructor uses all four control pins (AUX, M0, M1) plus the SoftwareSerial pointer.
// Signature: LoRa_E32(SoftwareSerial* serial, byte auxPin, byte m0Pin, byte m1Pin, UART_BPS_RATE bpsRate)
LoRa_E32 Transceiver(&LoRaSerial, PIN_AUX, PIN_M0, PIN_M1); 

// Load Cell, DHT22, Control Pins (D6 is still used by DHT)
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

// --- FIXED TRANSMISSION: STRUCTURED DATA DEFINITION ---
struct Telemetry_t {
    float thrust;
    float temperature;
    float humidity;
    bool isArmedStatus;
} currentTelemetry;

// --- ADDRESSING ---
const byte LP_ADDR_H = 0x00; 
const byte LP_ADDR_L = 0x00; 
const byte GS_ADDR_H = 0x00; 
const byte GS_ADDR_L = 0x01; 
const byte LORA_CHANNEL = 0x02; 

// --- FUNCTION PROTOTYPES ---
void executeCommand(char cmd);
void setSafePosition();
void setArmedPosition();
void readSensors();
void transmitTelemetry();


void setup() {
  Serial.begin(9600);
  
  // Initialize LoRa E32 Pins
  pinMode(PIN_M0, OUTPUT);
  pinMode(PIN_M1, OUTPUT);
  pinMode(PIN_AUX, INPUT); // AUX pin is now defined and set as input for stability
  
  // 1. Initialize E32 Transceiver
  LoRaSerial.begin(LORA_BAUD);
  
  // Transceiver.begin() uses the software serial and M0/M1 pins.
  Transceiver.begin(); 
  Serial.println("LoRa Transceiver Initialized.");


  // --- CONFIGURATION FOR FIXED TRANSMISSION ---
  ResponseStatus rs; 
  Configuration config;
  
  // Retrieve current configuration (or use defaults)
  ResponseStructContainer c = Transceiver.getConfiguration();
  if (c.status.code == E32_SUCCESS) {
    config = *(Configuration*)c.data;
  } 
  
  // Explicitly set the desired parameters using raw binary/hex values (FINAL FIX)
  config.SPED.airDataRate = 0b010;        // Air Data Rate: 2.4 kbps
  config.OPTION.transmissionPower = 0b01;  // Transmission Power: 17 dBm 
  config.SPED.uartParity = 0b00;           // UART Parity: 8N1 
  
  config.OPTION.fixedTransmission = FT_FIXED_TRANSMISSION; 
  config.ADDH = LP_ADDR_H; // Set module's own high address
  config.ADDL = LP_ADDR_L; // Set module's own low address
  config.CHAN = LORA_CHANNEL;

  
  // Save configuration permanently (0xC0 is the raw command for permanent save)
  rs = Transceiver.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE); 

  Serial.print("LoRa Config Set Status: ");
  Serial.println(rs.getResponseDescription());
  // --- END CONFIGURATION ---
  

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
  
  Serial.println("Launch Pad Ready (Fixed TX Mode).");
}

void loop() {
  // --- 1. HANDLE INCOMING COMMANDS ---
  // Note: Commands sent from GS are currently simple characters relayed via LoRaSerial
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
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("!!! LAUNCH SEQUENCE INITIATED !!!");
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
// --- TELEMETRY FUNCTIONS ---
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
  
  // Populate the structured message
  currentTelemetry.thrust = currentThrust_N;
  currentTelemetry.temperature = currentTemp_C;
  currentTelemetry.humidity = currentHumi_RH;
  currentTelemetry.isArmedStatus = isArmed;
}

void transmitTelemetry() {
  // Transmit the structured message to the Ground Station's fixed address (0x0001)
  ResponseStatus rs = Transceiver.sendFixedMessage(
    GS_ADDR_H, // Destination High Address
    GS_ADDR_L, // Destination Low Address
    LORA_CHANNEL, // Channel
    &currentTelemetry, 
    sizeof(Telemetry_t)
  );

  // Debug output via serial is crucial for verification
  if (rs.code != E32_SUCCESS) {
      Serial.print("TX Error: ");
      Serial.println(rs.getResponseDescription());
  } else {
      // FIX: Include all telemetry data in the debug output
      Serial.print("TX OK: Thrust=");
      Serial.print(currentTelemetry.thrust);
      Serial.print(" N, Temp=");
      Serial.print(currentTelemetry.temperature);
      Serial.print(" C, Humi=");
      Serial.print(currentTelemetry.humidity);
      Serial.println(" %");
  }
}
