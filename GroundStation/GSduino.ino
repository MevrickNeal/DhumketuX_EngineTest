/**
 * @file GS_Receiver.ino
 * @brief DhumketuX Ground Station Unit (GS) Firmware
 *
 * This sketch acts as a reliable, non-blocking bi-directional bridge between the
 * LoRa link (receiving binary telemetry) and the PC's USB Serial (for Web Serial UI).
 *
 * Features Added:
 * - Appends GS uptime timestamp to telemetry for Web UI link monitoring (>2s timeout).
 * - Updated 'I' command description to reflect 300ms ignition pulse requirement.
 *
 * Constraints: Strictly prohibits the use of the 'String' object.
 */

#include <SPI.h>
#include <LoRa.h> // Standard LoRa Library

// --- I. Configuration & Constants ---

// LoRa Pinout (Hardware SPI for speed)
#define LORA_CS_PIN         10 // D10: Chip Select (NSS)
#define LORA_RST_PIN        9  // D9: Reset
#define LORA_DIO0_PIN       2  // D2: DIO0 (Interrupt for packet detection)

// Status Feedback
#define STATUS_LED_PIN      7  // D7: Status/Alert LED
#define LED_FLASH_DURATION  50L // 50ms pulse for activity confirmation

// LoRa Parameters (Must match Launch Pad Unit (LPU) settings)
#define LORA_FREQUENCY      433E6
#define LORA_TX_POWER       20 // Max power (20dBm) for robustness

// System Timers
#define RX_CHECK_INTERVAL   10L  // Check for incoming LoRa packets every 10ms
#define TX_CHECK_INTERVAL   10L  // Check for incoming Serial commands every 10ms

// --- II. Data Structures ---

// Telemetry struct must match the LPU's packed struct exactly for reliable binary transfer
struct __attribute__((packed)) Telemetry_t {
    float thrust;
    float temperature;
    float humidity;
    int8_t isArmed;     // System state (0: Disarmed, 1: Armed, 2: Ignition)
    uint16_t checksum;
};

// --- III. Global Variables ---

// Non-blocking timer variables
unsigned long lastRxCheck = 0;
unsigned long lastTxCheck = 0;
unsigned long ledOffTime = 0; // Tracks when to turn the LED off
unsigned long lastTelemetryTime = 0; // NEW: Timestamp of the last successful telemetry packet received.

// Buffer for serial output (max length 64 bytes)
char serialBuffer[64];
Telemetry_t rx_telemetry;

// --- IV. Peripheral Management Functions ---

/**
 * @brief Toggles the status LED briefly to confirm communication activity.
 */
void flashStatusLED() {
    digitalWrite(STATUS_LED_PIN, HIGH);
    ledOffTime = millis() + LED_FLASH_DURATION;
}

/**
 * @brief Manages the status LED to turn it off after a short flash duration.
 */
void handleLEDStatus() {
    unsigned long currentMillis = millis();
    if (ledOffTime > 0 && currentMillis >= ledOffTime) {
        digitalWrite(STATUS_LED_PIN, LOW);
        ledOffTime = 0; // Reset timer
    }
}

/**
 * @brief Checks the received telemetry packet's checksum for integrity.
 * @param data Pointer to the Telemetry_t struct.
 * @return true if checksum is valid, false otherwise.
 */
bool verifyChecksum(const Telemetry_t* data) {
    uint16_t receivedChecksum = data->checksum;
    uint16_t calculatedSum = 0;
    const uint8_t* byteData = (const uint8_t*)data;

    // Calculate checksum over all bytes EXCEPT the last two (which hold the checksum itself)
    for (size_t i = 0; i < sizeof(Telemetry_t) - sizeof(uint16_t); ++i) {
        calculatedSum ^= byteData[i];
    }

    return receivedChecksum == calculatedSum;
}

// --- V. Bi-Directional Bridge Functions ---

/**
 * @brief Non-blocking check for incoming LoRa telemetry packets (LoRa RX -> PC TX).
 */
void checkLoRaRx() {
    int packetSize = LoRa.parsePacket();

    if (packetSize == sizeof(Telemetry_t)) {
        // Read the binary struct directly into the global variable
        LoRa.readBytes((uint8_t*)&rx_telemetry, sizeof(Telemetry_t));

        // 1. Checksum Verification
        if (!verifyChecksum(&rx_telemetry)) {
            Serial.println("WARN: Checksum failed.");
            return;
        }

        // 2. NEW: Record time of last successful packet
        lastTelemetryTime = millis(); 
        
        // 3. Telemetry Serialization (Strictly using C-style snprintf)
        // Format: "Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z,Time:TTTTTTTT\n"
        snprintf(serialBuffer, sizeof(serialBuffer),
                 "Thrust:%.2f,Temp:%.1f,Humi:%.1f,Time:%lu\n",
                 rx_telemetry.thrust,
                 rx_telemetry.temperature,
                 rx_telemetry.humidity,
                 lastTelemetryTime); // Added GS uptime timestamp for link monitoring

        // 4. Output to PC Serial
        Serial.print(serialBuffer);

        // 5. Feedback
        flashStatusLED();
    }
}

/**
 * @brief Non-blocking check for incoming Serial command characters (PC RX -> LoRa TX).
 */
void checkSerialTx() {
    // Check if any data is available on the USB Serial port
    if (Serial.available()) {
        char command = (char)Serial.read();

        // Check if the received character is a valid command
        if (command == 'A' || command == 'S' || command == 'T' || command == 'I') {
            
            // 1. LoRa Transmission
            LoRa.beginPacket();
            LoRa.write(command);
            LoRa.endPacket();

            // 2. Console Echo (Confirmation) using snprintf for C-style compliance
            if (command == 'A') { snprintf(serialBuffer, sizeof(serialBuffer), "TX Command: A (ARM)\n"); }
            else if (command == 'S') { snprintf(serialBuffer, sizeof(serialBuffer), "TX Command: S (SAFE)\n"); }
            else if (command == 'T') { snprintf(serialBuffer, sizeof(serialBuffer), "TX Command: T (TEST Telemetry)\n"); }
            // Command 'I' now reflects the 300ms pulse requirement
            else if (command == 'I') { snprintf(serialBuffer, sizeof(serialBuffer), "TX Command: I (IGNITE 300ms PULSE)\n"); } 
            
            Serial.print(serialBuffer);

            // 3. Feedback
            flashStatusLED();
        }
        // Drain any remaining buffered bytes, though typically only one is sent per command.
        while (Serial.available()) Serial.read();
    }
}

// --- VI. Setup and Loop ---

void setup() {
    // 1. Pin Initialization
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW); // Start with LED off

    // 2. Serial Initialization (Must match LPU speed for debugging)
    Serial.begin(115200);
    delay(500);
    Serial.println("\n*** DhumketuX GS Receiver Booting (Link Monitor Active) ***");

    // 3. LoRa Initialization (Fail-Safe Implementation)
    Serial.print("Initializing LoRa...");
    
    // Set explicit pins for the LoRa module
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    
    // Attempt to start LoRa module
    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("FATAL: LoRa INIT FAILED! Check wiring and module.");
        // Implement fail-safe halt with rapid flashing
        while (1) { 
            digitalWrite(STATUS_LED_PIN, HIGH); 
            delay(100); 
            digitalWrite(STATUS_LED_PIN, LOW); 
            delay(100);
        }
    }
    Serial.println("SUCCESS.");

    // Configure LoRa parameters (must match LPU for communication)
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSpreadingFactor(10);
    LoRa.setCodingRate4(5);
    LoRa.setSignalBandwidth(125E3);

    Serial.println("Ground Station Ready. Listening for Telemetry...");
}

void loop() {
    unsigned long currentMillis = millis();

    // 1. Non-blocking LoRa Reception Check
    if (currentMillis - lastRxCheck >= RX_CHECK_INTERVAL) {
        lastRxCheck = currentMillis;
        checkLoRaRx();
    }

    // 2. Non-blocking Serial Command Check
    if (currentMillis - lastTxCheck >= TX_CHECK_INTERVAL) {
        lastTxCheck = currentMillis;
        checkSerialTx();
    }
    
    // 3. LED Status Management
    handleLEDStatus();
}
