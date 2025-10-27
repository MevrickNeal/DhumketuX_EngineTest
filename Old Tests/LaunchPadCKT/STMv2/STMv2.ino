/**
 * @file LaunchPad_Tx_RadioLib_Perfected.ino
 * @brief DhumketuX Launch Pad Unit (LPU) Operational Firmware (STM32 Target)
 * * STATUS: **FIXED:** Switched HX711 library to use methods compatible with "HX711 by Rob Tillaart."
 * Architecture: Optimized for STM32 targets (e.g., Blue Pill) due to pin definitions (PAx, PBx).
 * * Constraints: Strictly no dynamic memory (String class prohibited). Uses C-style structs and non-blocking logic.
 */

#include <SPI.h>
#include <RadioLib.h>
#include <Servo.h>
#include <math.h>

// --- Sensor Libraries ---
#include <DHT.h>
// Using the HX711 library methods compatible with Rob Tillaart's version.
#include <HX711.h> 

// --- I. Configuration & Constants ---

// LoRa Module Configuration (RA-02) - Using STM32 Pin Names
#define LORA_FREQ_MHZ       433.0
#define LORA_CS_PIN         PA4
#define LORA_RST_PIN        PB5         // Stable RST Pin
#define LORA_DIO0_PIN       PB13

// Hardware Pinout (STM32)
#define LOADCELL_DOUT_PIN   PB0         // HX711 DOUT
#define LOADCELL_CLK_PIN    PB1         // HX711 SCK
#define SERVO_PIN           PA9         // Safety Servo PWM
#define IGNITION_RELAY_PIN  PC13        // Ignition Relay (Active LOW assumed: HIGH=OFF, LOW=ON)
#define ALERT_PIN           PB10        // Status/Alert LED
#define DHT_PIN             PA8         // DHT22 Data

// Sensor Configuration
#define DHT_TYPE            DHT22       // Using a DHT22 sensor
#define CALIBRATION_FACTOR  -400.0f     // !!! CRITICAL: Must be calibrated by the user (units/unit)

// Servo Limits
#define SERVO_LOCK_ANGLE    0           // Disarmed/Safe Position
#define SERVO_ARM_ANGLE     90          // Armed Position

// System Timing & Frequencies (Non-blocking control)
#define TELEMETRY_INTERVAL  100L        // Transmit struct every 100ms
#define SENSOR_READ_INTERVAL 1000L      // Read DHT sensor every 1 second
#define ALERT_FLASH_ARM     250L        // LED flash interval when ARMED
#define IGNITION_PULSE_MS   5000L       // 5.0 seconds duration for ignition sequence

// --- II. Data Structures & State Management ---

enum SystemState_t {
    STATE_DISARMED,
    STATE_ARMED,
    STATE_IGNITION_PULSE // Reflects 5.0s pulse
};

// Packed struct for efficient binary LoRa transmission
struct __attribute__((packed)) Telemetry_t {
    float thrust;       // Current thrust reading (calibrated units)
    float temperature;  // Environmental temperature
    float humidity;     // Environmental humidity
    int8_t isArmed;     // System state (0: Disarmed, 1: Armed, 2: Ignition)
    uint16_t checksum;  // Simple XOR checksum for data integrity
};

// --- III. Global Objects ---
// Note: Using the base SX1276 class for RA-02 modules
SX1276 radio = new Module(LORA_CS_PIN, LORA_DIO0_PIN, LORA_RST_PIN);
Servo safetyServo;
DHT dht(DHT_PIN, DHT_TYPE);
// USING: HX711 by Rob Tillaart
HX711 LoadCell;

// Global Variables
SystemState_t systemState = STATE_DISARMED;
Telemetry_t tx_telemetry;
unsigned long lastTelemetryTx = 0;
unsigned long lastSensorRead = 0;
unsigned long lastAlertToggle = 0;
unsigned long launchStartTime = 0;

// --- IV. Peripheral Management Functions ---

/**
 * @brief Calculates a simple XOR checksum for data integrity check.
 * @param data Pointer to the struct data.
 * @param length Length of the data to checksum (excluding the checksum field itself).
 * @return uint16_t The calculated checksum.
 */
uint16_t calculateChecksum(const void* data, size_t length) {
    uint16_t sum = 0;
    const uint8_t* byteData = (const uint8_t*)data;
    for (size_t i = 0; i < length; ++i) {
        sum ^= byteData[i];
    }
    return sum;
}

/**
 * @brief Reads the current calibrated thrust value from the load cell.
 * @return float Calibrated thrust data in defined units.
 */
float readLoadCell() {
    // FIX: Using Rob Tillaart's get_units(1) for the fastest single calibrated reading.
    // This is a synchronous (blocking) call, so we minimize the sampling time to 1.
    return LoadCell.get_units(1); 
}

/**
 * @brief Reads temperature and humidity from the DHT22 and updates the global struct.
 */
void readEnvironmentSensors() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    // Check for sensor reading failure
    if (isnan(t) || isnan(h)) {
        Serial.println("DHT Read Error! Using previous values.");
        // We do not overwrite current values if the read failed.
    } else {
        tx_telemetry.temperature = t;
        tx_telemetry.humidity = h;
    }
}

/**
 * @brief Controls the physical safety servo mechanism.
 * @param lockState True to lock (DISARM), False to arm (ARM).
 */
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

/**
 * @brief Provides visual feedback based on the current system state.
 */
void handleAlertFeedback() {
    unsigned long currentMillis = millis();
    unsigned long flashInterval = 0;

    if (systemState == STATE_ARMED) {
        flashInterval = ALERT_FLASH_ARM;
    } else if (systemState == STATE_IGNITION_PULSE) {
        // Rapid flash during ignition sequence
        flashInterval = 50L;
    } else {
        // STATE_DISARMED: LED off
        digitalWrite(ALERT_PIN, LOW);
        return;
    }

    if (currentMillis - lastAlertToggle >= flashInterval) {
        digitalWrite(ALERT_PIN, !digitalRead(ALERT_PIN));
        lastAlertToggle = currentMillis;
    }
}

// --- V. LoRa and State Functions ---

/**
 * @brief Manages the 5.0 second ignition pulse sequence.
 */
void handleIgnitionPulse() {
    if (systemState == STATE_IGNITION_PULSE) {
        unsigned long currentMillis = millis();
        // Check if the pulse duration has elapsed
        if (currentMillis - launchStartTime < IGNITION_PULSE_MS) {
            // Ignition ON (Active LOW)
            digitalWrite(IGNITION_RELAY_PIN, LOW);
        } else {
            // Ignition OFF (Active HIGH)
            digitalWrite(IGNITION_RELAY_PIN, HIGH);
            
            // Transition back to a safe state
            systemState = STATE_DISARMED;
            setSafety(true); // Re-lock safety servo
            Serial.println("Ignition Pulse Complete. System DISARMED.");
        }
    }
}

/**
 * @brief Packages and transmits the Telemetry_t struct via LoRa.
 */
void transmitTelemetry() {
    // 1. Update Load Cell 
    tx_telemetry.thrust = readLoadCell();
    
    // 2. Update status and checksum
    tx_telemetry.isArmed = (int8_t)systemState;
    tx_telemetry.checksum = calculateChecksum(&tx_telemetry, sizeof(Telemetry_t) - sizeof(uint16_t));

    // 3. Start Transmission 
    int state = radio.startTransmit((uint8_t*)&tx_telemetry, sizeof(Telemetry_t));
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("TX Error: "); Serial.println(state);
    }
    
    // CRITICAL: Block until transmit is finished to maintain half-duplex integrity.
    radio.finishTransmit(); 
}

/**
 * @brief Checks for incoming command packets via LoRa and handles state transitions.
 */
void checkLoRaCommand() {
    // Check if a packet has arrived in the internal buffer
    int packetLength = radio.startReceive();
    
    if (packetLength > 0) {
        uint8_t rxBuffer[1]; // Expecting only a single command character
        radio.readData(rxBuffer, 1);
        char command = (char)rxBuffer[0];
        
        // Ensure radio is ready for the next incoming packet
        radio.finishReceive(); 
        
        Serial.print("RX Command: "); Serial.println(command);

        switch (command) {
            case 'A': // Arm System
                if (systemState == STATE_DISARMED) {
                    systemState = STATE_ARMED;
                    setSafety(false); // Unlock servo
                    Serial.println("Command 'A': System ARMED.");
                }
                break;
                
            case 'S': // Safe/Disarm
                if (systemState != STATE_DISARMED) {
                    systemState = STATE_DISARMED;
                    setSafety(true); // Lock servo
                    digitalWrite(IGNITION_RELAY_PIN, HIGH); // Ensure relay is OFF
                    Serial.println("Command 'S': System DISARMED.");
                }
                break;
                
            case 'I': // Initiate 5.0s Ignition Pulse
                if (systemState == STATE_ARMED) {
                    systemState = STATE_IGNITION_PULSE;
                    launchStartTime = millis();
                    Serial.println("Command 'I': IGNITION PULSE (5.0s) INITIATED!");
                }
                break;
                
            case 'T': // Test/Quick Telemetry Request
                // Re-transmit immediately upon request, then return to receive mode
                transmitTelemetry();
                break;
        }
    } else if (packetLength < 0) {
         // Handle error cases during reception (e.g., CRC error)
        Serial.print("RX Error Code: "); Serial.println(packetLength);
    }
    
    // Always put radio back into continuous receive mode for command listening
    radio.startReceive();
}

// --- VI. Setup and Loop ---

void setup() {
    // 1. Pin Initialization
    pinMode(LORA_CS_PIN, OUTPUT);
    pinMode(LORA_RST_PIN, OUTPUT);
    pinMode(LORA_DIO0_PIN, INPUT);
    pinMode(ALERT_PIN, OUTPUT);
    pinMode(IGNITION_RELAY_PIN, OUTPUT);

    // 2. Safety defaults
    // Ignition OFF (HIGH) and Alert LED OFF (LOW)
    digitalWrite(IGNITION_RELAY_PIN, HIGH);
    digitalWrite(ALERT_PIN, LOW);

    // 3. Servo Initialization and Safety Lock
    safetyServo.attach(SERVO_PIN);
    setSafety(true); // Servo LOCKED (0 deg)

    // 4. Serial Setup
    Serial.begin(115200);
    delay(500);
    Serial.println("\n*** DhumketuX LPU Firmware Booting (Fixed HX711) ***");
    
    SPI.begin();

    // 5. LoRa Initialization 
    Serial.print("Initializing LoRa on PB5 RST pin...");
    int state = radio.begin(LORA_FREQ_MHZ);
    
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("FATAL: LoRa INIT FAILED! Error: "); Serial.println(state);
        while (1) { digitalWrite(ALERT_PIN, HIGH); delay(50); digitalWrite(ALERT_PIN, LOW); delay(500); }
    }
    Serial.println("LoRa Initialized (SUCCESS).");

    // Set stable LoRa parameters 
    radio.setBandwidth(125.0);
    radio.setSpreadingFactor(10);
    radio.setCodingRate(5);

    // 6. Sensor Initialization
    Serial.print("Initializing DHT22 Sensor...");
    dht.begin();
    Serial.println("OK.");

    Serial.print("Initializing Load Cell (HX711)...");
    // FIX: Using Rob Tillaart's begin method
    LoadCell.begin(LOADCELL_DOUT_PIN, LOADCELL_CLK_PIN);
    
    // Set scale for calibration factor
    LoadCell.set_scale(CALIBRATION_FACTOR); 
    
    // Perform zeroing
    // Uses the default taring method for Rob Tillaart's library
    LoadCell.tare(); 
    Serial.println("OK (Tared).");

    // Get initial baseline and start receiving commands immediately
    readEnvironmentSensors();
    radio.startReceive();
    Serial.println("System Ready. Waiting for 'A' (ARM) command.");
}

void loop() {
    unsigned long currentMillis = millis();

    // 1. Critical Ignition Sequence Manager
    handleIgnitionPulse();

    // 2. Non-blocking Sensor Read (DHT is slow and polled less frequently)
    if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = currentMillis;
        readEnvironmentSensors(); // Read DHT
    }

    // 3. Non-blocking Telemetry Transmission
    if (currentMillis - lastTelemetryTx >= TELEMETRY_INTERVAL) {
        lastTelemetryTx = currentMillis;
        transmitTelemetry();
    }

    // 4. Command Reception (Non-blocking)
    checkLoRaCommand();

    // 5. Visual Feedback
    handleAlertFeedback();
}
