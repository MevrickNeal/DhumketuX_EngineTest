/**
 * @file LaunchPad_Tx_RadioLib.ino
 * @brief DhumketuX LPU Final Operational Firmware
 * STATUS: Integrated live sensor readings for DHT22 (Temp/Humi) and HX711 (Load Cell).
 * PINOUT: LoRa RST on PB5.
 */

#include <SPI.h>
#include <RadioLib.h> 
#include <Servo.h> 
#include <math.h>

// --- Sensor Libraries ---
#include <DHT.h>
#include <HX711-ADC.h>

// --- I. Configuration & Constants ---

// LoRa Module Configuration (RA-02)
#define LORA_FREQ_MHZ       433.0       
#define LORA_CS_PIN         PA4         
#define LORA_RST_PIN        PB5         // Stable RST Pin
#define LORA_DIO0_PIN       PB13        

// Hardware Pinout
#define LOADCELL_DOUT_PIN   PB0         // HX711 DOUT
#define LOADCELL_CLK_PIN    PB1         // HX711 SCK
#define SERVO_PIN           PA9         
#define IGNITION_RELAY_PIN  PC13
#define ALERT_PIN           PB10
#define DHT_PIN             PA8         // DHT22 Data

// Sensor Configuration
#define DHT_TYPE            DHT22       // Using a DHT22 sensor

// Servo Limits
#define SERVO_LOCK_ANGLE    0           
#define SERVO_ARM_ANGLE     90          

// System Timing & Frequencies
#define TELEMETRY_INTERVAL  100L
#define SENSOR_READ_INTERVAL 1000L      // Read sensors every 1 second
#define ALERT_FLASH_ARM     250L
#define IGNITION_PULSE_MS   5000L       // 5.0 seconds

// --- II. Data Structures & State Management ---

enum SystemState_t {
    STATE_DISARMED,
    STATE_ARMED,
    STATE_LAUNCH_SEQUENCE
};

struct __attribute__((packed)) Telemetry_t {
    float thrust;       
    float temperature;  
    float humidity;     
    int8_t isArmed;     
    uint16_t checksum;  
};

// --- III. Global Objects ---
SX1276 radio = new Module(LORA_CS_PIN, LORA_DIO0_PIN, LORA_RST_PIN); 
Servo safetyServo;
DHT dht(DHT_PIN, DHT_TYPE);
HX711_ADC LoadCell(LOADCELL_DOUT_PIN, LOADCELL_CLK_PIN);

// Global Variables
SystemState_t systemState = STATE_DISARMED; 
Telemetry_t tx_telemetry;
volatile unsigned long lastTelemetryTx = 0;
volatile unsigned long lastSensorRead = 0;
volatile unsigned long lastAlertToggle = 0;
volatile unsigned long launchStartTime = 0;

// --- IV. Peripheral Management Functions ---

uint16_t calculateChecksum(const void* data, size_t length) {
    uint16_t sum = 0;
    const uint8_t* byteData = (const uint8_t*)data;
    for (size_t i = 0; i < length; ++i) {
        sum ^= byteData[i];
    }
    return sum;
}

// --- NEW: Live Sensor Reading ---
float readLoadCell() { 
    if (LoadCell.isReady()) {
        // Request a new reading
        LoadCell.startRead(); 
        // Get the value (e.g., in grams or kg, depends on your calibration factor)
        // You MUST calibrate this factor for accurate thrust readings.
        // float calibration_factor = 419.0; // Example: Set this with a known weight
        // return LoadCell.getData() / calibration_factor; 
        
        // For now, return raw data
        return LoadCell.getData();
    }
    return -99.9; // Return error code if not ready
}

// --- NEW: Live Sensor Reading ---
void readEnvironmentSensors() { 
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    // Check if any reading failed
    if (isnan(t) || isnan(h)) {
        Serial.println("DHT Read Error!");
        tx_telemetry.temperature = -99.9; 
        tx_telemetry.humidity = -99.9;
    } else {
        tx_telemetry.temperature = t;
        tx_telemetry.humidity = h;
    }
}

// --- Servo and Alert Functions (Unchanged) ---
void setSafety(bool lockState) { 
    if (!safetyServo.attached()) {
        safetyServo.attach(SERVO_PIN);
    }
    if (lockState) {
        safetyServo.write(SERVO_LOCK_ANGLE);
        Serial.print("Safety: LOCKED ("); Serial.print(SERVO_LOCK_ANGLE); Serial.println(" deg)");
    } else {
        safetyServo.write(SERVO_ARM_ANGLE);
        Serial.print("Safety: UNLOCKED/ARMED ("); Serial.print(SERVO_ARM_ANGLE); Serial.println(" deg)");
    }
}

void handleAlertFeedback() {
    unsigned long currentMillis = millis();
    if (systemState == STATE_ARMED) {
        if (currentMillis - lastAlertToggle >= ALERT_FLASH_ARM) {
            digitalWrite(ALERT_PIN, !digitalRead(ALERT_PIN)); 
            lastAlertToggle = currentMillis;
        }
    } else if (systemState == STATE_LAUNCH_SEQUENCE) {
        if (currentMillis - lastAlertToggle >= 50L) {
            digitalWrite(ALERT_PIN, !digitalRead(ALERT_PIN));
            lastAlertToggle = currentMillis;
        }
    } else {
        digitalWrite(ALERT_PIN, LOW);
    }
}

// --- LoRa and State Functions (Unchanged) ---
void handleLaunchSequence() {
    if (systemState == STATE_LAUNCH_SEQUENCE) {
        unsigned long currentMillis = millis();
        if (currentMillis - launchStartTime < IGNITION_PULSE_MS) {
            digitalWrite(IGNITION_RELAY_PIN, LOW); 
        } else {
            digitalWrite(IGNITION_RELAY_PIN, HIGH); 
            systemState = STATE_DISARMED;
            setSafety(true);
            Serial.println("Launch Sequence Complete. System DISARMED.");
        }
    }
}

void transmitTelemetry() {
    tx_telemetry.thrust = readLoadCell();
    tx_telemetry.isArmed = (int8_t)systemState; 
    tx_telemetry.checksum = calculateChecksum(&tx_telemetry, sizeof(Telemetry_t) - sizeof(uint16_t));
    
    // Transmit the struct
    radio.startTransmit((uint8_t*)&tx_telemetry, sizeof(Telemetry_t));
}

void checkLoRaCommand() {
    int packetLength = radio.available();
    if (packetLength > 0) {
        uint8_t rxBuffer[1]; 
        radio.readData(rxBuffer, 1);
        char command = (char)rxBuffer[0];

        switch (command) {
            case 'A': // Arm
                if (systemState == STATE_DISARMED) {
                    systemState = STATE_ARMED;
                    setSafety(false);
                    Serial.println("Command 'A': System ARMED.");
                }
                break;
            case 'S': // Safe/Disarm
                if (systemState != STATE_DISARMED) {
                    systemState = STATE_DISARMED;
                    setSafety(true);
                    digitalWrite(IGNITION_RELAY_PIN, HIGH);
                    Serial.println("Command 'S': System DISARMED.");
                }
                break;
            case 'I': // Initiate Launch Sequence
                if (systemState == STATE_ARMED) {
                    systemState = STATE_LAUNCH_SEQUENCE;
                    launchStartTime = millis();
                    Serial.println("Command 'I': LAUNCH SEQUENCE INITIATED!");
                }
                break;
            case 'T': // Test Telemetry
                transmitTelemetry();
                break;
        }
    }
}

// --- V. Setup and Loop ---

void setup() {
    // 1. Pin Initialization
    pinMode(LORA_CS_PIN, OUTPUT);
    pinMode(LORA_RST_PIN, OUTPUT); 
    pinMode(LORA_DIO0_PIN, INPUT); 
    pinMode(ALERT_PIN, OUTPUT);
    pinMode(IGNITION_RELAY_PIN, OUTPUT);
    
    // 2. Servo Initialization
    safetyServo.attach(SERVO_PIN);
    
    // 3. Safety defaults
    digitalWrite(IGNITION_RELAY_PIN, HIGH); 
    digitalWrite(ALERT_PIN, LOW);           
    setSafety(true);                        // Servo LOCKED (0 deg)

    // 4. Serial & Peripheral Setup
    Serial.begin(115200);
    delay(500); 
    Serial.println("\n*** DhumketuX LPU Firmware Booting (w/ Live Sensors) ***");
    
    SPI.begin(); 
    
    // 5. LoRa Initialization
    Serial.print("Initializing LoRa on PB5 RST pin...");
    int state = radio.beginFSK(); 
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("FATAL: LoRa INIT FAILED! Error: "); Serial.println(state);
        while (1) { digitalWrite(ALERT_PIN, HIGH); delay(50); digitalWrite(ALERT_PIN, LOW); delay(500); } 
    }
    Serial.println("LoRa Initialized (SUCCESS).");
    radio.setFrequency(LORA_FREQ_MHZ);
    radio.setBandwidth(125.0);
    radio.setSpreadingFactor(10);
    radio.setCodingRate(5);
    
    // 6. Sensor Initialization
    Serial.print("Initializing DHT22 Sensor...");
    dht.begin();
    Serial.println("OK.");

    Serial.print("Initializing Load Cell (HX711)...");
    LoadCell.begin();
    // LoadCell.setSamples(10); // Optional: smooth out readings
    LoadCell.tare(); // Zero the scale at boot
    Serial.println("OK (Tared).");
    
    radio.startReceive(); 
    readEnvironmentSensors(); // Get initial baseline
    Serial.println("System Ready. Waiting for 'A' (ARM) command.");
}

void loop() {
    unsigned long currentMillis = millis();
    
    // 1. Critical Launch Sequence
    handleLaunchSequence(); 

    // 2. Non-blocking Sensor Read
    if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = currentMillis;
        readEnvironmentSensors(); // Read DHT
        // Note: Load cell is read just-in-time during transmitTelemetry()
    }

    // 3. Non-blocking Telemetry Tx
    if (currentMillis - lastTelemetryTx >= TELEMETRY_INTERVAL) {
        lastTelemetryTx = currentMillis;
        transmitTelemetry();  // This now sends real sensor data
        radio.startReceive(); 
    }
    
    // 4. Command Reception
    checkLoRaCommand();

    // 5. Visual/Audio Feedback
    handleAlertFeedback();
}
