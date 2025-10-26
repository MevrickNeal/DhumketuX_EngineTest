You got it. As your embedded lead, I'm providing the remaining two complete, synchronized code files: the Ground Station (GS) Firmware and the Web App UI (HTML/JS).

This set completes the entire DhumketuX system, fully integrated with the new state machine logic for the 30-second countdown, the 2x arm beeps, and the 4-second test tone.

1. Ground Station (GS) Firmware (GS_Receiver.ino) 🛰️
This firmware is crucial for mapping the Web UI's intended actions (e.g., clicking 'LAUNCH') to the correct single-character command expected by the LPU's new state machine (L, T, A, S).

Code snippet

/**
 * @file GS_Receiver.ino
 * @brief DhumketuX Ground Station Unit (GS) Firmware
 *
 * STATUS: Synchronized with LPU for the new command map (A, S, T, L) and link monitoring.
 *
 * Architecture: Arduino Uno/Nano. Constraints: No String object, non-blocking logic.
 */

#include <SPI.h>
#include <LoRa.h>

// --- Configuration ---
#define LORA_FREQ_MHZ       433.0
#define LORA_CS_PIN         10
#define LORA_RST_PIN        9
#define LORA_DIO0_PIN       2
#define STATUS_LED_PIN      7 // Status LED Pin

// System Timing
#define FEEDBACK_PULSE_MS   50L // Brief pulse duration for LED confirmation
#define BUF_SIZE            100 // Buffer size for C-style string creation

// --- Data Structures ---
// NOTE: This must match the LPU's packed struct exactly
struct __attribute__((packed)) Telemetry_t {
    float thrust;
    float temperature;
    float humidity;
    int8_t isArmed; // 0=Disarmed, 1=Armed, 2=Countdown, 3=Ignition
    uint16_t checksum;
};

// --- Global Variables ---
char txBuf[BUF_SIZE];
unsigned long lastActivityTime = 0; // For GS D7 LED feedback

// --- Helper Functions ---

// C-style function to serialize the struct into a single string
void serializeTelemetry(const Telemetry_t* telemetry) {
    unsigned long time_ms = millis();
    
    // Format: Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z,Time:TTTTTTTT,Armed:B\r\n
    snprintf(txBuf, BUF_SIZE, "Thrust:%.2f,Temp:%.1f,Humi:%.1f,Time:%lu,Armed:%d\r\n", 
             telemetry->thrust, 
             telemetry->temperature, 
             telemetry->humidity,
             time_ms,
             telemetry->isArmed); // Pass the LPU's internal state (0, 1, 2, 3)
    
    Serial.write(txBuf);
}

// Function to handle brief LED flash on any activity (RX or TX)
void handleStatusLed(unsigned long currentMillis) {
    if (currentMillis - lastActivityTime < FEEDBACK_PULSE_MS) {
        digitalWrite(STATUS_LED_PIN, HIGH);
    } else {
        digitalWrite(STATUS_LED_PIN, LOW);
    }
}

// --- LoRa Functions ---

void checkLoRaCommand() {
    if (Serial.available() > 0) {
        char command = Serial.read();
        
        // Allowed commands are: A (ARM), S (SAFE), T (TEST TONE/PULSE), L (LAUNCH Countdown), I (Immediate Ignition, only used by UI for auto-safe).
        if (command == 'A' || command == 'S' || command == 'T' || command == 'L' || command == 'I') {
            LoRa.beginPacket();
            LoRa.write(command);
            LoRa.endPacket(true); // Blocking send, wait for completion
            
            lastActivityTime = millis(); // Trigger LED flash for TX
            
            // Console Echo for debug
            Serial.print("TX Command: ");
            Serial.write(command);
            Serial.println();
        }
    }
}

void checkLoRaTelemetry() {
    int packetSize = LoRa.parsePacket();
    
    if (packetSize == sizeof(Telemetry_t)) {
        Telemetry_t rx_telemetry;
        
        LoRa.readBytes((uint8_t*)&rx_telemetry, sizeof(Telemetry_t));
        
        lastActivityTime = millis(); // Trigger LED flash for RX
        
        serializeTelemetry(&rx_telemetry);
    }
}

// --- Setup and Loop ---

void setup() {
    Serial.begin(115200);
    delay(500);

    // 1. Pin Initialization
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // 2. LoRa Initialization
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    
    Serial.print("*** DhumketuX GS Receiver Booting ***\nInitializing LoRa...");
    // LoRa.begin expects frequency in Hertz (Hz), multiply MHz by 1000000
    if (!LoRa.begin(LORA_FREQ_MHZ * 1000000)) { 
        Serial.println("FATAL: LoRa Init Failed!");
        while (1) { /* Halt */ }
    }
    LoRa.setSpreadingFactor(10);
    LoRa.setCodingRate4(5);
    LoRa.setTxPower(20);
    Serial.println("SUCCESS.\nGround Station Ready. Listening for Telemetry...");
}

void loop() {
    unsigned long currentMillis = millis();
    
    checkLoRaTelemetry();
    checkLoRaCommand();
    
    handleStatusLed(currentMillis);
}
