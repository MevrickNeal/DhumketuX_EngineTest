#include <SPI.h> // Required for LoRa module communication
#include <LoRa.h> // The standard LoRa library

// I. ARCHITECTURE & CONSTRAINTS:
// Strict adherence to C-style strings (char arrays) is maintained.
// The String class is explicitly prohibited to ensure high efficiency and prevent memory fragmentation.
// All logic is non-blocking (except the initial LoRa setup halt).

// II. HARDWARE & PINOUT:
const int CS_PIN = 10;     // LoRa SPI NSS/CS -> D10 (Hardware SPI SS)
const int RST_PIN = 9;     // LoRa SPI RST -> D9
const int IRQ_PIN = 2;     // LoRa SPI DIO0 (Interrupt) -> D2
const int STATUS_LED_PIN = 8; // Status LED -> D8

// LoRa Configuration Parameters
const long LORA_FREQUENCY = 433E6; // LoRa frequency (e.g., 433MHz)
const int TX_POWER = 14;           // LoRa transmission power (dBm)
const int SPREADING_FACTOR = 10;   // LoRa spreading factor (SF)
const int SYNC_WORD = 0x12;        // Common sync word for LoRa network

// III. PROTOCOL & BI-DIRECTIONAL BRIDGE:
// The Telemetry struct must match the Launch Pad's definition exactly for binary compatibility.
typedef struct {
    float thrust_kPa;
    float temperature_C;
    float humidity_perc;
} Telemetry_t;

// IV. FAIL-SAFE FEEDBACK & LED BLINKING LOGIC:
const unsigned long LED_BLINK_DURATION = 50; // Milliseconds to keep the LED on for a visual confirmation
unsigned long ledOffTime = 0; // Tracks the time when the LED should be turned off

// Buffer for serializing Telemetry data (must be large enough for the string format)
// Format: "Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z" (approx 40 chars needed)
const size_t SERIAL_BUFFER_SIZE = 64;
char serialBuffer[SERIAL_BUFFER_SIZE];

/**
 * @brief Triggers the status LED to blink briefly.
 * This function is non-blocking. The LED state is managed by updateStatusLED().
 */
void triggerStatusLED() {
    digitalWrite(STATUS_LED_PIN, HIGH);
    ledOffTime = millis() + LED_BLINK_DURATION;
}

/**
 * @brief Manages the non-blocking turning off of the status LED.
 */
void updateStatusLED() {
    if (digitalRead(STATUS_LED_PIN) == HIGH && millis() > ledOffTime) {
        digitalWrite(STATUS_LED_PIN, LOW);
    }
}

/**
 * @brief Implements non-blocking checks for LoRa packet reception and serialization.
 * (LoRa RX -> PC Serial TX)
 */
void handleLoRaRx() {
    // Check if a packet has arrived
    int packetSize = LoRa.parsePacket();

    if (packetSize) {
        // 1. Validate packet size against the expected struct size
        if (packetSize == sizeof(Telemetry_t)) {
            Telemetry_t telemetry;

            // 2. Read the entire binary struct directly into the local variable
            LoRa.readBytes((byte*)&telemetry, sizeof(Telemetry_t));

            // 3. Serialize data into the required C-string format
            // Format: "Thrust:XX.XX,Temp:YY.Y,Humi:ZZ.Z"
            // snprintf is used instead of the String object or concatenation.
            int len = snprintf(
                serialBuffer,
                SERIAL_BUFFER_SIZE,
                "Thrust:%.2f,Temp:%.1f,Humi:%.1f",
                telemetry.thrust_kPa,
                telemetry.temperature_C,
                telemetry.humidity_perc
            );

            if (len > 0 && len < SERIAL_BUFFER_SIZE) {
                // 4. Transmit serialized data over USB Serial
                Serial.println(serialBuffer);
                triggerStatusLED(); // Signal successful RX
            } else {
                // Handle serialization error
                Serial.println("RX-Error: Buffer overflow or serialization failed.");
            }
        } else {
            // Packet size mismatch: read remaining bytes to clear the buffer
            while (LoRa.available()) {
                LoRa.read();
            }
            Serial.println("RX-Error: Size mismatch.");
        }
    }
}

/**
 * @brief Implements non-blocking checks for PC Serial command reception and LoRa transmission.
 * (PC Serial RX -> LoRa TX)
 */
void handleSerialTx() {
    // Check for incoming data in the Serial buffer
    if (Serial.available() > 0) {
        char command = Serial.read();

        // Check if the received character is one of the valid commands
        if (command == 'A' || command == 'S' || command == 'T' || command == 'I') {

            // 1. Begin LoRa packet (unaddressed)
            LoRa.beginPacket(0);

            // 2. Write the single command character
            LoRa.write(command);

            // 3. End packet and transmit (true ensures a non-blocking transmission check)
            if (LoRa.endPacket(true)) {
                Serial.print("TX Command Sent: ");
                Serial.println(command);
                triggerStatusLED(); // Signal successful TX
            } else {
                Serial.println("TX-Error: LoRa transmission failed.");
            }
        } else {
            // Acknowledge receipt of an unhandled character
            Serial.print("TX-Error: Unknown Command (");
            Serial.print(command);
            Serial.println(").");
        }
    }
}


void setup() {
    // 1. Initialize Serial Communication
    Serial.begin(115200);
    // Wait for serial port to connect (required for Arduino Leonardo/Micro)
    while (!Serial);
    Serial.println("DhumketuX GS Receiver Initializing...");

    // 2. Configure Pin Modes
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // 3. LoRa Setup (with fail-safe halt)
    LoRa.setPins(CS_PIN, RST_PIN, IRQ_PIN);

    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("LoRa initialization FAILED. HALTING MCU.");
        // FAIL-SAFE: Halt the MCU and rapidly blink the status LED
        while (1) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(100);
            digitalWrite(STATUS_LED_PIN, LOW);
            delay(100);
        }
    }

    // Apply additional LoRa settings
    LoRa.setTxPower(TX_POWER);
    LoRa.setSpreadingFactor(SPREADING_FACTOR);
    LoRa.setSyncWord(SYNC_WORD);

    Serial.println("LoRa Ground Station Ready.");
}

void loop() {
    // Non-blocking check for incoming LoRa packets and serial transmission
    handleLoRaRx();

    // Non-blocking check for incoming Serial commands and LoRa transmission
    handleSerialTx();

    // Non-blocking update of the Status LED state
    updateStatusLED();
}
