#include <Adafruit_HX711.h>
#include <Servo.h>
#include <SPI.h>
#include <LoRa.h>

enum launchState {
  IDLE,
  ARMED,
  LAUNCHED
};

launchState state = IDLE;

//======for LoRa Connection=======
#define NSS_PIN PA4
#define RST_PIN PB12
#define DIO0_PIN PB13


//========physical IO========
#define buzzer PB10
#define relay PA0
#define button PA2
#define servo_pin PA3
#define led PC13

//==============for weight scale==========
#define weight_scale 2878
#define sample_count 10


//=========variables for load cell=======
int tare = 0;
int avg_tare = 0;
float weight;
bool calibrated = 0;


//========declaring objects========
Servo servo;
Adafruit_HX711 hx711(PB0, PB1);

//=========variables for the timer=======
uint32_t timer;


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("LaunchPad Initializing...");
  LoRa.setPins(NSS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }
  pinMode(led, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, 1);
  pinMode(button, INPUT);
  servo.attach(servo_pin);
  servo.write(90);  //change the direction if needed==================================
  hx711.begin();
  Serial.println("Launch Pad Ready!");
}


void loop() {
  incoming();

  byte r = push();
  if (r) {
    Serial.println(r);
    if (r == 1) calibrate_scale();
    else if (r == 2) while (!push()) check_weight();
  }

  if (state == LAUNCHED) {
    if (millis() - timer > 100) {
      timer = millis();
      sendStatus();
    }
  }

  else if (state == ARMED) {
    if (millis() - timer > 1000) {
      beep(2);
      timer = millis();
    }
  }
}
