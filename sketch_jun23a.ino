#define BLYNK_TEMPLATE_ID "TMPL6TiWXIBAu"
#define BLYNK_TEMPLATE_NAME "Trush Bin"
#define BLYNK_AUTH_TOKEN "AOyoNZ-zSMDw59xQXpsibgvonyIVenLs"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>

Servo myServo;
BlynkTimer timer;

char ssid[] ="Bdconnect";
char pass[] ="01762603";

int distance;
int level;
int fullBinCount = 0;
int isDoorLocked = 0;

#define TRIG1 4
#define ECHO1 16
#define TRIG2 17
#define ECHO2 5
#define LED_PIN 2
#define BUZZER_PIN 22
#define SERVO_PIN 21

#define BIN_FULL 5.0
#define MAX_DIST 11.00
#define MIN_DIST 5

#define VIRTUAL_PIN_LEVEL V0
#define VIRTUAL_PIN_BIN_STATUS V1
#define VIRTUAL_PIN_DOOR_SWITCH V2


// --- Function Prototypes ---
long getDistance(int trigPin, int echoPin);
long getDistance2(int trigPin, int echoPin);
int calculateLevel(float dist, int virtualPin);
void doorOpen();
void doorClose();

void setup() {
  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  myServo.attach(SERVO_PIN); 
  myServo.write(0);

  timer.setInterval(1000L, binMain);
  Serial.println("Trush Bin system ready");
}


// --- Main Logic Loop ---
void loop() {
  Blynk.run();
  timer.run();
}

// --- Blynk App Door Switch Control ---
BLYNK_WRITE(VIRTUAL_PIN_DOOR_SWITCH) {
  int switchState = param.asInt();
  isDoorLocked = switchState == 1;
  Serial.println(isDoorLocked ? "Manual LOCK enabled" : "Manual UNLOCK enabled");
}
// --- Main Trash Bin Function ---
void binMain() {  
  float d2 = getDistance2(TRIG2, ECHO2);

  // If door is not locked and hand detected — open door
  if (!isDoorLocked && d2 <= 30) {
    Serial.println("Opening door...");
    doorOpen();
    Serial.println("Door Opened...");
  }
  
  else if (!isDoorLocked) {
    Serial.println("Closing door...");
    doorClose();
    Serial.println("Door Closed...");
  }

  float d1 = getDistance(TRIG1, ECHO1); // this is for distance level
  distance = calculateLevel(d1, VIRTUAL_PIN_LEVEL);

  Serial.print("Sensor 1 (Bin Level): ");
  Serial.print(d1);
  Serial.print(" cm → Level: ");
  Serial.print(distance);
  Serial.println("%");

  Serial.print("Sensor 2 (Hand Detection): ");
  Serial.print(d2);
  Serial.println(" cm");

  if (d1 <= BIN_FULL) {
    Serial.println("Trush Bin is full!");
    // Blynk.logEvent("bin_full", "Trush Bin is full!");

    // ====== Alarm when bin is full ========
    // digitalWrite(LED_PIN, HIGH);
    binAlarm();

    fullBinCount++;

    if (fullBinCount >= 5 && !isDoorLocked) {
      Serial.println("Bin FULL confirmed 5x, locking...");
      Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, HIGH); // notify app
      Blynk.virtualWrite(VIRTUAL_PIN_DOOR_SWITCH, 1);   // Lock switch ON

      isDoorLocked = true;
      fullBinCount = 0; 
    } 
  } 
  else {
    fullBinCount = 0;
    digitalWrite(LED_PIN, LOW);
    Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, LOW);
  }

  Serial.println("----------");
}

// --- Distance Calculation ---
long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long dur = pulseIn(echoPin, HIGH);
  return dur * 0.034 / 2;
}
// --- Distance Calculation ---
long getDistance2(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long dur = pulseIn(echoPin, HIGH);
  return dur * 0.034 / 2;
}

// --- Level Calculation ---
int calculateLevel(float dist, int virtualPin) {
  dist = constrain(dist, MIN_DIST, MAX_DIST);
  float levelPercent = 100.0 * (MAX_DIST - dist) / (MAX_DIST - MIN_DIST);
  level = round(levelPercent);

  Blynk.virtualWrite(virtualPin, level);
  return level;
}

// --- Servo Control ---
void doorOpen() {
    myServo.write(90);
}

void doorClose() {
  myServo.write(0);
}

void binAlarm() {
  tone(BUZZER_PIN, 1000);
  delay(500);
  noTone(BUZZER_PIN);
}
