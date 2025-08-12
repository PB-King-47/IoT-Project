#define BLYNK_TEMPLATE_ID "TMPL6TiWXIBAu"
#define BLYNK_TEMPLATE_NAME "Trush Bin"
#define BLYNK_AUTH_TOKEN "AOyoNZ-zSMDw59xQXpsibgvonyIVenLs"


#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <HCSR04.h>

Servo myServo;
BlynkTimer timer;

char ssid[] = "Bdconnect";
char pass[] = "01762603";

// Pins
#define TRIG1 13
#define ECHO1 12
#define TRIG2 14
#define ECHO2 27
#define LED_PIN 2
#define BUZZER_PIN 25
#define SERVO_PIN 26

// Constants
#define BIN_FULL_THRESHOLD 5.0
#define MAX_DISTANCE 18.67
#define MIN_DISTANCE 5.0
#define HAND_DETECT_DISTANCE 30.00

// Virtual Pins
#define VIRTUAL_PIN_LEVEL V0
#define VIRTUAL_PIN_BIN_STATUS V1
#define VIRTUAL_PIN_DOOR_SWITCH V2
#define VIRTUAL_PIN_DOOR_STATUS V3

// State Variables
int level;
int fullBinCount = 0;
bool isDoorLocked = false;
bool isManualLockEnabled = false;
bool doorCurrentlyOpen = false;
bool triggerAlarm = false;

HCSR04 binLevelSensor(TRIG1, ECHO1);
HCSR04 handDetectSensor(TRIG2, ECHO2);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  myServo.attach(SERVO_PIN);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(500L, binMain);
  
  Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, LOW);
  Blynk.virtualWrite(VIRTUAL_PIN_DOOR_SWITCH, 0);
  Blynk.virtualWrite(VIRTUAL_PIN_DOOR_STATUS, 0);

  myServo.write(0);

  Serial.println("Smart Trash Bin Ready");
}

void loop() {
  Blynk.run();
  timer.run();
}

BLYNK_WRITE(VIRTUAL_PIN_DOOR_SWITCH) {
  int switchState = param.asInt();
  isManualLockEnabled = (switchState == 1);

  if (!isManualLockEnabled) {
    Serial.println("Manual UNLOCK");
    fullBinCount = 0;
    isDoorLocked = false;
  } else {    
    Serial.println("Manual LOCK");
    isDoorLocked = true;
  }
}

void binMain() {
  float binLevelDistance = binLevelSensor.dist();
  float handDistance = handDetectSensor.dist();

  level = calculateLevel(binLevelDistance, VIRTUAL_PIN_LEVEL);

  Serial.print("Bin Level: ");
  Serial.print(binLevelDistance);
  Serial.print(" cm (");
  Serial.print(level);
  Serial.print("%) | Hand: ");
  Serial.print(handDistance);
  Serial.print(" cm | ");

  Serial.println("");
  Serial.print("isDoorLocked: ");
  Serial.println(isDoorLocked);
  Serial.print("isManualLockEnabled: ");
  Serial.println(isManualLockEnabled);
  Serial.print("doorCurrentlyOpen: ");
  Serial.println(doorCurrentlyOpen);

  // Check full
  if (binLevelDistance <= BIN_FULL_THRESHOLD) {
      digitalWrite(LED_PIN, HIGH); // LED ON
      fullBinCount++;

      if (fullBinCount >= 3 && !isManualLockEnabled) {
          isManualLockEnabled = true;   // enable manual lock mode
          isDoorLocked = true;          // keep door closed
          doorCurrentlyOpen = false;    // reset open state
          
          // in this here i need the alarm trigger for at 1s time
          binAlarm();

          Blynk.virtualWrite(VIRTUAL_PIN_DOOR_SWITCH, HIGH);
          Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, HIGH);
          Serial.println("BIN FULL → AUTO LOCKED & DOOR CLOSED");
      } else {
          Serial.println("BIN FULL → MANUAL UNLOCK Or Lock it.");
      }

  } else {
      fullBinCount = 0;
      digitalWrite(LED_PIN, LOW);
      Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, LOW);
  }

  // Motion detection
  if (handDistance > 0 && handDistance <= HAND_DETECT_DISTANCE) {
    if (isManualLockEnabled) {  
        Serial.println("Hand detected but DOOR LOCKED");  
        // Do nothing — keep door closed
    }
    else if (isDoorLocked && !doorCurrentlyOpen) {
        Serial.println("Hand detected → Door OPEN");
        isDoorLocked = false; // open door
    }

} else {
    if (doorCurrentlyOpen && !isManualLockEnabled) {
        Serial.println("Hand not detected → Door Close");
        isDoorLocked = true; // close door
    }
}


  if(isDoorLocked == true) {doorClose();}
  else{doorOpen();}

  Serial.println("----------");
}

int calculateLevel(float dist, int virtualPin) {
  dist = constrain(dist, MIN_DISTANCE, MAX_DISTANCE);
  float levelPercent = 100.0 * (MAX_DISTANCE - dist) / (MAX_DISTANCE - MIN_DISTANCE);
  level = round(levelPercent);
  Blynk.virtualWrite(virtualPin, level);
  return level;
}

void binAlarm() {
  tone(BUZZER_PIN, 1000);
  delay(500);
  noTone(BUZZER_PIN);
}

void doorOpen() {
    myServo.write(90);
    delay(3000);
    doorCurrentlyOpen = true;
    Blynk.virtualWrite(VIRTUAL_PIN_DOOR_STATUS, 1);
}

void doorClose() {
    myServo.write(0);
    delay(3000);
    doorCurrentlyOpen = false;
    Blynk.virtualWrite(VIRTUAL_PIN_DOOR_STATUS, 0);
}