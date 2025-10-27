#include <Adafruit_HX711.h>
#include <Servo.h>
// #include <DHT.h>


//========physical IO========
#define buzzer PB10
#define relay PA0
#define button PA2
// #define DHT_PIN PB15
#define servo_pin PA3
#define led PC13

//==============for weight scale==========
#define weight_scale 2878
#define sample_count 10


//=========variables for load cell=======
int tare = 0;
int avg_tare = 0;
float weight;


//========declaring objects========
Servo servo;
// DHT dht(DHT_PIN, DHT22);
Adafruit_HX711 hx711(PB0, PB1);


void setup() {
  Serial.begin(115200);
  pinMode(led, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, 1);
  pinMode(button, INPUT);
  servo.attach(servo_pin);
  servo.write(90);
  hx711.begin();
  Serial.println("Hello");
  // dht.begin();
}


void loop() {

  byte r = push();
  if (r) {
    Serial.println(r);
    if (r == 1) calibrate_scale();
    else if (r == 2) check_weight();
  }
}
