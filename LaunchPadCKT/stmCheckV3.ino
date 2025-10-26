/**
 * @file LaunchPad_Tx_LoRa_Final.ino
 * @brief DhumketuX Launch Pad Unit (LPU) FINAL FIRMWARE - SYNCHRONIZED
 *
 * STATUS: Switched to standard <LoRa.h> and ASCII String transmission for compatibility with your working setup.
 * All state management (tones, countdown) remains non-blocking.
 */

#include <SPI.h>
#include <LoRa.h> // CRITICAL FIX: Switched back to standard LoRa library
#include <Servo.h>
#include <math.h>

// --- Sensor Libraries ---
#include <DHT.h>
#include <HX711.h> 
#include <Wire.h> // Needed for I2C if sensors used MPU/BME style

// --- I. Configuration & Constants ---

// LoRa Pinout (STM32 Pin Names - MATCHING YOUR FC PINS)
#define LORA_CS_PIN         PA4
#define LORA_RST_PIN        PA3 // CRITICAL FIX: Match the FC's RST pin
#define LORA_DIO0_PIN       PA2

// Hardware Pinout (STM32) - Retaining previous peripheral pins
#define LOADCELL_DOUT_PIN   PB0
#define LOADCELL_CLK_PIN    PB1
#define SERVO_PIN           PA9
#define IGNITION_RELAY_PIN  PC13        
#define ALERT_PIN           PB10        // Status LED / Buzzer Tone Pin
#define DHT_PIN             PA8         

// ... [Remaining configuration constants remain the same]
#define LORA_FREQ_MHZ       433E6       // Note: LoRa.begin() expects Hz
#define DHT_TYPE            DHT22
#define CALIBRATION_FACTOR  -400.0f     
#define SERVO_LOCK_ANGLE    0
#define SERVO_ARM_ANGLE     90
#define TELEMETRY_INTERVAL  100L
#define SENSOR_READ_INTERVAL 1000L
#define COMMAND_FEEDBACK_MS 50L         
#define IGNITION_PULSE_MS   300L        
#define HANDSHAKE_BEEP_DURATION 500L    

// --- II. Data Structures & State Management ---

enum SystemState_t {
    STATE_DISARMED,         
    STATE_ARMED,            
    STATE_TEST_TONE,        
    STATE_COUNTDOWN,        
    STATE_IGNITION_PULSE    
};

// NOTE: Telemetry_t struct is REMOVED. We will send an ASCII string.

// --- III. Global Objects & Variables ---
// Removed RadioLib SX1276 object. LoRa library uses global functions.
Servo safetyServo;
DHT dht(DHT_PIN, DHT_TYPE);
HX711 LoadCell;

SystemState_t systemState = STATE_DISARMED;
// Retaining data variables for sending via string
float tx_thrust = 0;
float tx_temp = 0;
float tx_humi = 0;

unsigned long lastTelemetryTx = 0;
unsigned long lastSensorRead = 0;
unsigned long commandRxTime = 0;
unsigned long sequenceStartTime = 0;
unsigned long lastToneToggle = 0;
char txStringBuffer[80]; // Buffer for ASCII output

// --- Musical Notes (Frequencies in Hz) ---
#define NOTE_C4  262
// ... [other note definitions remain the same] ...

// --- IV. Helper Functions ---
// [Checksum function is removed as we are no longer sending binary]
float readLoadCell() { return LoadCell.get_units(1); }

void readEnvironmentSensors() {
    tx_temp = dht.readTemperature();
    tx_humi = dht.readHumidity();
}
void setSafety(bool lockState) {
    if (!safetyServo.attached()) { safetyServo.attach(SERVO_PIN); }
    if (lockState) { safetyServo.write(SERVO_LOCK_ANGLE); } 
    else { safetyServo.write(SERVO_ARM_ANGLE); }
    Serial.print("Safety: "); Serial.println(lockState ? "LOCKED (SAFE)" : "UNLOCKED (ARMED)");
}

// ... [playTone and playHappyBirthday remain the same] ...

// --- V. State Management & Timed Sequences ---
// [handleAuditorySequences remains the same]
void handleAuditorySequences(unsigned long currentMillis) {
    // ... [Logic for STATE_TEST_TONE, STATE_COUNTDOWN, STATE_IGNITION_PULSE remains identical] ...
    noTone(ALERT_PIN); // Stop tone by default unless explicitly started below

    if (systemState == STATE_TEST_TONE) {
        if (currentMillis - sequenceStartTime < 4000L) {
            tone(ALERT_PIN, 880); 
            digitalWrite(ALERT_PIN, HIGH);
        } else {
            digitalWrite(IGNITION_RELAY_PIN, HIGH); 
            systemState = STATE_ARMED;
            Serial.println("Test fire sequence complete. Armed.");
        }
    } 
    else if (systemState == STATE_COUNTDOWN) {
        unsigned long elapsed = currentMillis - sequenceStartTime;
        long remaining = 30000L - elapsed;
        
        if (remaining <= 0) {
            systemState = STATE_IGNITION_PULSE; 
            return;
        }
        
        unsigned long toneInterval = (remaining > 10000L) ? 1000L : 200L;
        if (currentMillis - lastToneToggle >= toneInterval) {
            if (remaining > 50L) { 
                tone(ALERT_PIN, (remaining > 10000L) ? 500 : 1500, 100);
            }
            lastToneToggle = currentMillis;
        }
    }
    else if (systemState == STATE_IGNITION_PULSE) {
        if (currentMillis - sequenceStartTime < IGNITION_PULSE_MS) {
            digitalWrite(IGNITION_RELAY_PIN, LOW); 
            tone(ALERT_PIN, 3000); 
            digitalWrite(ALERT_PIN, HIGH);
        } else {
            digitalWrite(IGNITION_RELAY_PIN, HIGH); 
            noTone(ALERT_PIN);
            systemState = STATE_DISARMED;
            setSafety(true); 
            Serial.println("Ignition Pulse Complete. System AUTO-DISARMED.");
        }
    }
}

/**
 * @brief Transmits data as an ASCII string using the standard LoRa.print().
 */
void transmitTelemetry() {
    // 1. Get live data
    tx_thrust = readLoadCell(); 
    
    // 2. Format the CSV string (Thrust, Temp, Humi, ArmedState)
    int armedState = (systemState == STATE_IGNITION_PULSE) ? 3 : (systemState == STATE_COUNTDOWN ? 2 : (systemState == STATE_ARMED ? 1 : 0));
    
    // Use snprintf to create the C-style string
    snprintf(txStringBuffer, sizeof(txStringBuffer), "%.2f,%.1f,%.1f,%d", 
             tx_thrust, tx_temp, tx_humi, armedState);

    // 3. Send the packet using LoRa.print()
    LoRa.beginPacket();
    LoRa.print(txStringBuffer); 
    LoRa.endPacket(true); // Blocking send, necessary for robust ACK/clearance

    // NOTE: Telemetry is now ONLY sent when the interval dictates, solving the link loss issue.
}

/**
 * @brief Checks for incoming command packets via standard LoRa.parsePacket().
 */
void checkLoRaCommand() {
    unsigned long currentMillis = millis(); 
    int packetSize = LoRa.parsePacket();

    if (packetSize) {
        char command = (char)LoRa.read(); // Read single character command
        
        commandRxTime = millis(); // Quick pulse feedback trigger
        
        // Drain the rest of the packet buffer
        while(LoRa.available()) LoRa.read();
        
        Serial.print("RX Command: "); Serial.println(command);

        switch (command) {
            case 'Z': // Connection Handshake Beep
                tone(ALERT_PIN, 1000, HANDSHAKE_BEEP_DURATION); 
                digitalWrite(ALERT_PIN, HIGH);
                Serial.println("Handshake received. Single beep confirm.");
                break;
            // ... [Remaining command logic (A, S, L, T) remains the same] ...
            case 'A': // ARM
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
            case 'L': // Start 30s Countdown
                if (systemState == STATE_ARMED) {
                    systemState = STATE_COUNTDOWN;
                    sequenceStartTime = currentMillis; 
                    lastToneToggle = currentMillis; 
                    Serial.println("Command 'L': 30s COUNTDOWN INITIATED.");
                }
                break;
            case 'T': // Test Fire (300ms pulse + 4s tone)
                if (systemState == STATE_ARMED) {
                    systemState = STATE_TEST_TONE;
                    sequenceStartTime = currentMillis; 
                    Serial.println("Command 'T': TEST PULSE (300ms) + 4s Tone.");
                }
                break;
        }
    }
}

// --- VI. Setup and Loop ---

void setup() {
    // 1. Pin Initialization
    pinMode(ALERT_PIN, OUTPUT);
    pinMode(IGNITION_RELAY_PIN, OUTPUT);
    digitalWrite(IGNITION_RELAY_PIN, HIGH);
    digitalWrite(ALERT_PIN, LOW);

    // 2. Peripheral Setup
    Serial.begin(115200);
    LoadCell.begin(LOADCELL_DOUT_PIN, LOADCELL_CLK_PIN);
    LoadCell.set_scale(CALIBRATION_FACTOR); 
    LoadCell.tare(); 
    setSafety(true);
    
    // *** STARTUP MELODY EXECUTION (Synchronous) ***
    playHappyBirthday();

    // 3. LoRa Setup (Uses the standard LoRa library)
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    if (!LoRa.begin(LORA_FREQ_MHZ)) { 
        Serial.println("FATAL: LoRa Init Failed!");
        while (1) { /* Halt */ }
    }
    LoRa.setSpreadingFactor(10);
    LoRa.setCodingRate4(5);
    LoRa.setTxPower(20);

    readEnvironmentSensors();
    // LoRa.receive() is not used; reception is handled by parsePacket in checkLoRaCommand.

    Serial.println("\n*** LPU Ready for Command. Waiting for Handshake ('Z'). ***");
}

void loop() {
    unsigned long currentMillis = millis(); 
    
    // 1. State-driven Timed Sequences (Auditory and Ignition)
    handleAuditorySequences(currentMillis);

    // 2. Non-blocking Sensor Read
    if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = currentMillis;
        readEnvironmentSensors(); 
    }

    // 3. Non-blocking Telemetry Transmission
    if (currentMillis - lastTelemetryTx >= TELEMETRY_INTERVAL) {
        lastTelemetryTx = currentMillis;
        transmitTelemetry();
    }

    // 4. Command Reception
    checkLoRaCommand();
    
    // 5. Quick Command RX/TX Blink (50ms flash)
    if (currentMillis - commandRxTime < COMMAND_FEEDBACK_MS && systemState < STATE_TEST_TONE) {
        digitalWrite(ALERT_PIN, HIGH);
    } 
    else if (systemState < STATE_TEST_TONE) {
        noTone(ALERT_PIN);
        digitalWrite(ALERT_PIN, LOW);
    }
}
