// DhumketuX Engine Test System - Launch Pad Unit (LP)
// TARGET MCU: STM32F103C8T6 (Bluepill)
// WIRELESS: LoRa RA-02 (SPI Interface)
// Role: Reads sensors, executes commands, and transmits structured telemetry via SPI.

#include "HX711.h"
#include <DHT.h>       
#include <Adafruit_Sensor.h> 
#include <Servo.h>     
#include <SPI.h>       // Include SPI library
#include <LoRa.h>      // Standard LoRa library for RA-02 modules

// --- LoRa RA-02 PIN DEFINITIONS (STM32 GPIO) ---
// SPI Pins (Standard SPI1 on Bluepill)
#define SS_PIN    PA4   // LoRa Chip Select (NSS)
#define RST_PIN   PB12  // LoRa Reset
#define DIO0_PIN  PB13  // LoRa DIO0 (Interrupt for received packet)

// LoRa Frequency (MUST be 433E6 for RA-02 modules)
const long LORA_FREQUENCY = 433E6; 

// --- STM32 GPIO for Sensors/Controls ---
const int LOADCELL_DOUT_PIN = PB11; 
const int LOADCELL_SCK_PIN = PB10;  
const long CALIBRATION_FACTOR = 456000; 
HX711 scale;
#define DHTPIN PB1     
#define DHTTYPE DHT22  
DHT dht(DHTPIN, DHTTYPE);
const int RELAY_PIN = PC13; 
const int SERVO_PIN = PB0;  
Servo safetyServo;

// --- STRUCTURED DATA DEFINITION ---
// This struct will be sent as a raw byte array (Fixed Transmission equivalent)
struct Telemetry_t {
    float thrust;
    float temperature;
    float humidity;
    bool isArmedStatus;
} currentTelemetry;

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
void receiveCommand(); // New function for SPI command listening

void setup() {
  // Serial1 (USB/Native) for PC Debugging on STM32
  Serial.begin(9600); 
  
  // 1. Initialize LoRa SPI
  pinMode(SS_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(DIO0_PIN, INPUT); 

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa INIT FAILED! Check connections.");
    while (true); // Halt on critical failure
  }
  LoRa.setSyncWord(0x12); // Unique Sync Word for DhumketuX (0x12 is common)
  Serial.println("LoRa Transceiver Initialized (RA-02 SPI).");
  
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
  
  Serial.println("Launch Pad Ready (STM32 LoRa TX).");
}

void loop() {
  // --- 1. HANDLE INCOMING COMMANDS (Via SPI LoRa) ---
  receiveCommand();

  // --- 2. READ & TRANSMIT TELEMETRY (Every 100ms) ---
  readSensors();
  transmitTelemetry();

  delay(100); 
}

// -------------------------------------------------------------------
// --- COMMUNICATION FUNCTIONS ---
// -------------------------------------------------------------------

void transmitTelemetry() {
  // 1. Start packet transmission
  LoRa.beginPacket(); 
  
  // 2. Write the raw struct bytes to the packet
  LoRa.write((byte*)&currentTelemetry, sizeof(Telemetry_t));
  
  // 3. End packet and transmit
  int result = LoRa.endPacket();

  // Debug output
  if (result) {
      Serial.print("TX OK: Thrust=");
      Serial.print(currentTelemetry.thrust);
      Serial.print(", Humi=");
      Serial.println(currentTelemetry.humidity);
  } else {
      Serial.println("TX Error: LoRa Send Failed (Check LoRa.begin() Status).");
  }
}

void receiveCommand() {
  // Check if a command packet has been received
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    // We expect a single character command ('A', 'S', 'T', 'I')
    if (packetSize == 1) {
      char command = (char)LoRa.read();
      executeCommand(command);
    } else {
      Serial.print("RX WARN: Received non-command packet size: ");
      Serial.println(packetSize);
      // Consume the rest of the buffer to clear the packet
      while(LoRa.available()) { LoRa.read(); }
    }
  }
}

// -------------------------------------------------------------------
// --- CONTROL AND TELEMETRY FUNCTIONS (Logic Adapted) ---
// -------------------------------------------------------------------

void executeCommand(char cmd) {
  // ... (Implementation remains the same, but uses currentTelemetry.isArmedStatus directly) ...
  Serial.print("Command Received: ");
  Serial.println(cmd);

  switch (cmd) {
    case 'A': 
      setArmedPosition();
      currentTelemetry.isArmedStatus = true;
      Serial.println("System ARMED.");
      break;
      
    case 'S': 
      setSafePosition();
      currentTelemetry.isArmedStatus = false;
      Serial.println("System DISARMED.");
      break;

    case 'T': 
      if (currentTelemetry.isArmedStatus) {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("TEST FIRE: IGNITION ON (300ms)");
        delay(300); 
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("TEST FIRE: IGNITION OFF");
      } else {
        Serial.println("IGNITION BLOCKED: Not ARMED.");
      }
      break;

    case 'I': 
      if (currentTelemetry.isArmedStatus) {
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
}
