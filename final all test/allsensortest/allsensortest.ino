/**
 * DhumketuX LPU - HARDWARE TEST SKETCH
 * Goal: Test all connected sensors and actuators individually.
 * Outputs status and readings to the Serial Monitor (115200 baud).
 *
 * Components Tested:
 * - DHT22 (Temp/Humi) on PB15 (Updated)
 * - HX711 (Load Cell) on PB0/PB1
 * - Servo (MG995) on PA9
 * - Ignition Relay on PC13 (Active LOW)
 * - Alert/Buzzer on PB10
 */

#include <Servo.h>
#include <DHT.h>
#include <HX711.h> // Rob Tillaart Library

// --- Pin Definitions (Match your LPU hardware) ---
#define HX_DOUT_PIN      PB0
#define HX_SCK_PIN       PB1

#define DHT_PIN          PB15 // <<< UPDATED PIN
#define DHT_TYPE         DHT22

#define SERVO_PIN        PA9
#define IGNITION_RELAY_PIN PC13
#define ALERT_PIN        PB10

// --- Sensor/Actuator Objects ---
DHT dht(DHT_PIN, DHT_TYPE);
HX711 hx;
Servo testServo;

// --- Calibration (Use placeholder or your value for scaled readings) ---
long  hx_offset = 0;
// Use the scale factor you calculated previously, or 1.0f to see raw counts
float hx_scale  = 0.9748f; // Replace with your calculated value if known

// --- Servo Test Parameters ---
#define SERVO_LOCK_ANGLE    0
#define SERVO_ARM_ANGLE     90
#define SERVO_SWEEP_DELAY 15 // Delay between servo steps (ms)

void setup() {
  Serial.begin(115200);
  delay(2000); // Wait for Serial Monitor connection
  Serial.println("\n--- LPU Hardware Test Initializing ---");

  // --- Initialize DHT ---
  Serial.print("Initializing DHT22 on PB15..."); // <<< UPDATED MESSAGE
  dht.begin();
  float initial_temp = dht.readTemperature(); // Attempt first read
  if (isnan(initial_temp)) {
      Serial.println(" FAILED initial read! Check wiring/power.");
  } else {
      Serial.println(" OK.");
  }

  // --- Initialize HX711 ---
  Serial.print("Initializing HX711 on PB0/PB1...");
  hx.begin(HX_DOUT_PIN, HX_SCK_PIN);
  Serial.print(" Taring (keep scale empty)...");
  if (hx.is_ready()) {
      hx.tare(20); // Tare
      hx_offset = hx.get_offset();
      hx.set_scale(hx_scale); // Set scale
      Serial.println(" OK.");
      Serial.print("  Offset: "); Serial.println(hx_offset);
      Serial.print("  Scale Factor Used: "); Serial.println(hx_scale);
  } else {
      Serial.println(" FAILED during tare! Check HX711 wiring/power.");
  }

  // --- Initialize Servo ---
  Serial.print("Initializing Servo on PA9...");
  testServo.attach(SERVO_PIN);
  testServo.write(SERVO_LOCK_ANGLE); // Start at lock position
  Serial.println(" OK.");

  // --- Initialize Relay and Alert Pins ---
  Serial.print("Setting up Relay (PC13) and Alert (PB10)...");
  pinMode(IGNITION_RELAY_PIN, OUTPUT);
  digitalWrite(IGNITION_RELAY_PIN, HIGH); // Relay OFF (Active LOW)
  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW); // Alert OFF
  Serial.println(" OK.");

  Serial.println("\n--- Starting Hardware Test Cycle ---");
}

void loop() {
  Serial.println("\n----- New Test Cycle -----");

  // --- 1. Test DHT22 ---
  Serial.println("Testing DHT22:");
  float tempC = dht.readTemperature();
  float humiP = dht.readHumidity();
  if (isnan(tempC) || isnan(humiP)) {
    Serial.println("  DHT Read FAILED!");
  } else {
    Serial.print("  Temperature: "); Serial.print(tempC, 1); Serial.println(" *C");
    Serial.print("  Humidity:    "); Serial.print(humiP, 1); Serial.println(" %");
  }
  delay(1000);

  // --- 2. Test HX711 ---
  Serial.println("Testing HX711:");
  if (hx.is_ready()) {
    long rawValue = hx.read_average(5); // Raw ADC counts
    float scaledValue = hx.get_units(5); // Scaled value (grams if calibrated)
    Serial.print("  Raw ADC Value: "); Serial.println(rawValue);
    Serial.print("  Scaled Value (grams): "); Serial.println(scaledValue, 2);
  } else {
    Serial.println("  HX711 Not Ready!");
  }
  delay(1000);

  // --- 3. Test Servo ---
  Serial.println("Testing Servo Sweep (MG995):");
  Serial.print("  Sweeping to ARM position ("); Serial.print(SERVO_ARM_ANGLE); Serial.println(" deg)...");
  for (int pos = SERVO_LOCK_ANGLE; pos <= SERVO_ARM_ANGLE; pos += 1) {
    testServo.write(pos);
    delay(SERVO_SWEEP_DELAY);
  }
  Serial.println("  Reached ARM position.");
  delay(1000);
  Serial.print("  Sweeping back to LOCK position ("); Serial.print(SERVO_LOCK_ANGLE); Serial.println(" deg)...");
  for (int pos = SERVO_ARM_ANGLE; pos >= SERVO_LOCK_ANGLE; pos -= 1) {
    testServo.write(pos);
    delay(SERVO_SWEEP_DELAY);
  }
  Serial.println("  Reached LOCK position.");
  delay(1000);

  // --- 4. Test Relay ---
  Serial.println("Testing Ignition Relay (PC13):");
  Serial.println("  Turning Relay ON (Active LOW)...");
  digitalWrite(IGNITION_RELAY_PIN, LOW); // Relay ON
  delay(2000); // Keep it on for 2 seconds
  Serial.println("  Turning Relay OFF...");
  digitalWrite(IGNITION_RELAY_PIN, HIGH); // Relay OFF
  delay(1000);

  // --- 5. Test Alert/Buzzer ---
  Serial.println("Testing Alert/Buzzer (PB10):");
  Serial.println("  Beeping at 1kHz for 1 second...");
  tone(ALERT_PIN, 1000, 1000); // 1kHz tone for 1000ms
  digitalWrite(ALERT_PIN, HIGH); // Turn on LED as well
  delay(1100); // Wait slightly longer than tone duration
  digitalWrite(ALERT_PIN, LOW); // Turn off LED
  noTone(ALERT_PIN); // Ensure tone is stopped
  Serial.println("  Beep complete.");

  Serial.println("----- Test Cycle Complete -----");
  delay(3000); // Wait 3 seconds before next cycle
}

