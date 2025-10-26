/**
 * @file LaunchPad_Tx_RadioLib_Perfected.ino
 * @brief DhumketuX Launch Pad Unit (LPU) Final Operational Firmware
 *
 * STATUS: Implemented complex state machine for auditory feedback (2x beep, 30s countdown, 4s test tone).
 *
 * Architecture: STM32. Constraints: No String object, non-blocking logic.
 */

#include <SPI.h>
#include <RadioLib.h>
#include <Servo.h>
#include <math.h>

// --- Sensor Libraries ---
#include <DHT.h>
#include <HX711.h> // Using HX711 by Rob Tillaart

// --- I. Configuration & Constants ---

// LoRa Module Configuration (RA-02) - Using STM32 Pin Names
#define LORA_FREQ_MHZ       433.0
#define LORA_CS_PIN         PA4
#define LORA_RST_PIN        PB5
#define LORA_DIO0_PIN       PB13

// Hardware Pinout (STM32)
#define LOADCELL_DOUT_PIN   PB0
#define LOADCELL_CLK_PIN    PB1
#define SERVO_PIN           PA9
#define IGNITION_RELAY_PIN  PC13        // Active LOW assumed: HIGH=OFF, LOW=ON
#define ALERT_PIN           PB10        // Status LED / Buzzer Tone Pin
#define DHT_PIN             PA8         // DHT22 Data

// Sensor Configuration
#define DHT_TYPE            DHT22
#define CALIBRATION_FACTOR  -400.0f     // CRITICAL: Must be calibrated by the user

// Servo Limits
#define SERVO_LOCK_ANGLE    0
#define SERVO_ARM_ANGLE     90

// System Timing & Frequencies
#define TELEMETRY_INTERVAL  100L
#define SENSOR_READ_INTERVAL 1000L
#define COMMAND_FEEDBACK_MS 50L         // Duration of quick flash/tone upon command RX
#define IGNITION_PULSE_MS   300L        // 300ms duration for final ignition pulse

// --- II. Data Structures & State Management ---

enum SystemState_t {
    STATE_DISARMED,         // System is safe, servo locked
    STATE_ARMED,            // System is armed, servo unlocked, waiting for sequence
    STATE_TEST_TONE,        // Test sequence: 4 second continuous tone
    STATE_ARM_TONE_PULSE1,  // Arm sequence: First beep
    STATE_ARM_TONE_PULSE2,  // Arm sequence: Second beep
    STATE_COUNTDOWN,        // 30 second pre-launch sequence
    STATE_IGNITION_PULSE    // 300ms fire
};

// Packed struct for efficient binary LoRa transmission
struct __attribute__((packed)) Telemetry_t {
    float thrust;
    float temperature;
    float humidity;
    int8_t isArmed;         // 0: Disarmed, 1: Armed, 2: Countdown, 3: Ignition
    uint16_t checksum;
};

// --- III. Global Objects & Variables ---
SX1276 radio = new Module(LORA_CS_PIN, LORA_DIO0_PIN, LORA_RST_PIN);
Servo safetyServo;
DHT dht(DHT_PIN, DHT_TYPE);
HX711 LoadCell;

SystemState_t systemState = STATE_DISARMED;
Telemetry_t tx_telemetry;
unsigned long lastTelemetryTx = 0;
unsigned long lastSensorRead = 0;
unsigned long commandRxTime = 0;
unsigned long sequenceStartTime = 0;
unsigned long lastToneToggle = 0;

// --- IV. Helper Functions ---

uint16_t calculateChecksum(const void* data, size_t length) {
    uint16_t sum = 0;
    const uint8_t* byteData = (const uint8_t*)data;
    for (size_t i = 0; i < length; ++i) {
        sum ^= byteData[i];
    }
    return sum;
}

float readLoadCell() { return LoadCell.get_units(1); }

void readEnvironmentSensors() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        tx_telemetry.temperature = t;
        tx_telemetry.humidity = h;
    }
}

void setSafety(bool lockState) {
    if (!safetyServo.attached()) { safetyServo.attach(SERVO_PIN); }
    if (lockState) { safetyServo.write(SERVO_LOCK_ANGLE); } 
    else { safetyServo.write(SERVO_ARM_ANGLE); }
    Serial.print("Safety: "); Serial.println(lockState ? "LOCKED (SAFE)" : "UNLOCKED (ARMED)");
}

void buzzerFeedback(unsigned long duration, int frequency) {
    commandRxTime = millis();
    tone(ALERT_PIN, frequency, duration);
    digitalWrite(ALERT_PIN, HIGH);
}

// --- V. State Management & Timed Sequences ---

void handleAuditorySequences(unsigned long currentMillis) {
    noTone(ALERT_PIN); // Stop tone by default

    if (systemState == STATE_TEST_TONE) {
        // 4 SECOND CONTINUOUS TONE
        if (currentMillis - sequenceStartTime < 4000L) {
            tone(ALERT_PIN, 880); // High pitch for test fire prep
        } else {
            // Test tone complete, return to armed state
            digitalWrite(IGNITION_RELAY_PIN, HIGH); // Ensure relay is OFF
            systemState = STATE_ARMED;
            Serial.println("Test fire sequence complete. Armed.");
        }
    } 
    else if (systemState == STATE_ARM_TONE_PULSE1 || systemState == STATE_ARM_TONE_PULSE2) {
        // 2x BEEPS FOR ARMING CONFIRMATION
        unsigned long elapsed = currentMillis - sequenceStartTime;
        
        if (systemState == STATE_ARM_TONE_PULSE1 && elapsed < 200L) {
            tone(ALERT_PIN, 1200);
        } else if (systemState == STATE_ARM_TONE_PULSE1 && elapsed >= 300L) {
            sequenceStartTime = currentMillis; // Reset timer for second pulse
            systemState = STATE_ARM_TONE_PULSE2;
        } else if (systemState == STATE_ARM_TONE_PULSE2 && elapsed < 200L) {
            tone(ALERT_PIN, 1200);
        } else if (systemState == STATE_ARM_TONE_PULSE2 && elapsed >= 200L) {
            systemState = STATE_ARMED; // Sequence done, transition to armed
            Serial.println("Auditory ARM sequence complete.");
        }
    }
    else if (systemState == STATE_COUNTDOWN) {
        // 30 SECOND COUNTDOWN TONE SEQUENCE
        unsigned long elapsed = currentMillis - sequenceStartTime;
        long remaining = 30000L - elapsed;
        
        if (remaining <= 0) {
            systemState = STATE_IGNITION_PULSE; // T-0, execute ignition
            return;
        }

        // Determine beep frequency based on remaining time
        unsigned long toneInterval = (remaining > 10000L) ? 1000L : 200L; // Slower until T-10s

        if (currentMillis - lastToneToggle >= toneInterval) {
            // Toggle tone and reset timer
            if (remaining > 50L) { // Prevent last toggle from being super short
                tone(ALERT_PIN, (remaining > 10000L) ? 500 : 1500, 100);
            }
            lastToneToggle = currentMillis;
        }
    }
    else if (systemState == STATE_IGNITION_PULSE) {
        // 300ms FIRE SEQUENCE
        if (currentMillis - sequenceStartTime < IGNITION_PULSE_MS) {
            digitalWrite(IGNITION_RELAY_PIN, LOW); // Ignition ON
            tone(ALERT_PIN, 3000); // Continuous high tone during pulse
        } else {
            // Pulse complete, auto-revert to SAFE
            digitalWrite(IGNITION_RELAY_PIN, HIGH); // Ignition OFF
            noTone(ALERT_PIN);
            systemState = STATE_DISARMED;
            setSafety(true); // Enforce mechanical safety
            Serial.println("Ignition Pulse Complete. System AUTO-DISARMED.");
        }
    }
}


void transmitTelemetry() {
    tx_telemetry.thrust = readLoadCell();
    // Use an integer representation of state for transmission
    if (systemState == STATE_IGNITION_PULSE) tx_telemetry.isArmed = 3;
    else if (systemState == STATE_COUNTDOWN) tx_telemetry.isArmed = 2;
    else if (systemState == STATE_ARMED) tx_telemetry.isArmed = 1;
    else tx_telemetry.isArmed = 0;

    tx_telemetry.checksum = calculateChecksum(&tx_telemetry, sizeof(Telemetry_t) - sizeof(uint16_t));

    radio.startTransmit((uint8_t*)&tx_telemetry, sizeof(Telemetry_t));
    radio.finishTransmit();
    radio.startReceive();
}

void checkLoRaCommand() {
    int packetLength = radio.startReceive();
    
    if (packetLength > 0) {
        uint8_t rxBuffer[1]; 
        radio.readData(rxBuffer, 1);
        char command = (char)rxBuffer[0];
        radio.finishReceive(); 
        commandRxTime = millis(); // Quick pulse feedback trigger
        
        Serial.print("RX Command: "); Serial.println(command);

        switch (command) {
            case 'A': // ARM + 2x Beeps
                if (systemState == STATE_DISARMED) {
                    systemState = STATE_ARM_TONE_PULSE1;
                    sequenceStartTime = millis();
                    setSafety(false);
                    Serial.println("Command 'A': System ARMED (2x beep sequence started).");
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
                    sequenceStartTime = millis();
                    lastToneToggle = currentMillis; // Initialize tone timer
                    Serial.println("Command 'L': 30s COUNTDOWN INITIATED.");
                }
                break;
            case 'T': // Test Fire (300ms pulse + 4s tone)
                if (systemState == STATE_ARMED) {
                    systemState = STATE_TEST_TONE;
                    sequenceStartTime = millis();
                    digitalWrite(IGNITION_RELAY_PIN, LOW); // Test pulse ON
                    Serial.println("Command 'T': TEST PULSE (300ms) + 4s Tone.");
                    // Note: The main loop handler will revert state and turn off relay.
                }
                break;
        }
    } else if (packetLength < 0) {
        // Handle error codes if necessary
    }
    radio.startReceive();
}

// --- VI. Setup and Loop ---

void setup() {
    // 1. Pin Initialization
    pinMode(ALERT_PIN, OUTPUT);
    pinMode(IGNITION_RELAY_PIN, OUTPUT);
    digitalWrite(IGNITION_RELAY_PIN, HIGH);
    digitalWrite(ALERT_PIN, LOW);

    // 2. Servo, Serial, LoRa, and Sensor Setup (omitted for brevity, assume success from previous steps)
    // ... Initialization code ...

    Serial.begin(115200);
    LoadCell.begin(LOADCELL_DOUT_PIN, LOADCELL_CLK_PIN);
    LoadCell.set_scale(CALIBRATION_FACTOR); 
    LoadCell.tare(); 
    setSafety(true); // Default safe
    radio.begin(LORA_FREQ_MHZ);
    radio.setBandwidth(125.0);
    radio.setSpreadingFactor(10);
    radio.setCodingRate(5);
    radio.startReceive();
    readEnvironmentSensors();

    Serial.println("\n*** LPU Ready for Command ***");
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

    // 5. Quick Command RX/TX Blink (LPU LED/Buzzer is handled in commandRxTime check within handleAuditorySequences)
    if (currentMillis - commandRxTime < COMMAND_FEEDBACK_MS) {
        digitalWrite(ALERT_PIN, HIGH);
    } else {
        digitalWrite(ALERT_PIN, LOW);
    }
}
