/**
 * @file GS_Receiver.ino
 * @brief DhumketuX Ground Station Unit (GS) Firmware
 *
 * STATUS: FIXED: Telemetry output format changed to pure CSV (Comma Separated Values) for efficiency.
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
// Reduced buffer size for the more compact CSV format
#define BUF_SIZE            60 

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

// C-style function to serialize the struct into a single CSV string
void serializeTelemetry(const Telemetry_t* telemetry) {
    unsigned long time_ms = millis();
    
    // NEW CSV FORMAT: XX.XX,YY.Y,ZZ.Z,TTTTTTTT,B\r\n
    // (Thrust, Temp, Humi, Time, ArmedState)
    snprintf(txBuf, BUF_SIZE, "%.2f,%.1f,%.1f,%lu,%d\r\n", 
             telemetry->thrust, 
             telemetry->temperature, 
             telemetry->humidity,
             time_ms,
             telemetry->isArmed); 
    
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

// --- LoRa Functions (Omitted for brevity, no change) ---
void checkLoRaCommand() {
    if (Serial.available() > 0) {
        char command = Serial.read();
        
        if (command == 'A' || command == 'S' || command == 'T' || command == 'L' || command == 'I') {
            LoRa.beginPacket();
            LoRa.write(command);
            LoRa.endPacket(true); 
            
            lastActivityTime = millis(); 
            
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
        
        lastActivityTime = millis(); 
        
        serializeTelemetry(&rx_telemetry);
    }
}

// --- Setup and Loop (Omitted for brevity, no change) ---

void setup() {
    Serial.begin(115200);
    delay(500);

    // 1. Pin Initialization
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // 2. LoRa Initialization
    LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
    
    Serial.print("*** DhumketuX GS Receiver Booting ***\nInitializing LoRa...");
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
