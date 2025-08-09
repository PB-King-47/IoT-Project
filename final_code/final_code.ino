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
#define TRIG1 4
#define ECHO1 16
#define TRIG2 17
#define ECHO2 5
#define LED_PIN 2
#define BUZZER_PIN 22
#define SERVO_PIN 21

// Constants
#define BIN_FULL_THRESHOLD 3.0
#define MAX_DISTANCE 11.0
#define MIN_DISTANCE 5.0
#define HAND_DETECT_DISTANCE 30

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
bool manualUnlockAfterFull = false;

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
  myServo.write(0);
  delay(4000);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(500L, binMain);

  Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, LOW);
  Blynk.virtualWrite(VIRTUAL_PIN_DOOR_SWITCH, 0);
  Blynk.virtualWrite(VIRTUAL_PIN_DOOR_STATUS, 0);

  Serial.println("Smart Trash Bin Ready");
}

void loop() {
  Blynk.run();
  timer.run();
}

BLYNK_WRITE(VIRTUAL_PIN_DOOR_SWITCH) {
  int switchState = param.asInt();
  isManualLockEnabled = (switchState == 1);

  if (isManualLockEnabled) {
    Serial.println("Manual UNLOCK");
    fullBinCount = 0;
    isDoorLocked = false;
    manualUnlockAfterFull = true;  // Prevent auto-lock until emptied
    doorOpen();
  } else {
    Serial.println("Manual LOCK");
    doorClose();
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

  // Check full
  if (binLevelDistance <= BIN_FULL_THRESHOLD) {
    digitalWrite(LED_PIN, HIGH);
    fullBinCount++;

    if (!isManualLockEnabled && !isDoorLocked && !manualUnlockAfterFull) {
      if (fullBinCount >= 5) {
        isDoorLocked = true;
        doorClose();
        Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, HIGH);
        Blynk.logEvent("bin_full", "Trash Bin is full and auto-locked!");
        Serial.println("BIN FULL → AUTO LOCKED");
      }
    } else {
      Serial.println("BIN FULL → MANUAL UNLOCK OR ALREADY LOCKED");
    }
  } else {
    fullBinCount = 0;
    digitalWrite(LED_PIN, LOW);
    Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, LOW);

    if (manualUnlockAfterFull) {
      manualUnlockAfterFull = false;  // Reset only when bin becomes empty
      Serial.println("Bin emptied → Manual unlock flag reset");
    }

    if (!isManualLockEnabled) {
      isDoorLocked = false;
    }
  }

  // Motion detection
  if (handDistance > 0 && handDistance <= HAND_DETECT_DISTANCE) {
    if (!isDoorLocked) {
      if (!doorCurrentlyOpen) {
        Serial.println("Hand detected → Door OPEN");
        doorOpen();
      }
    } else {
      Serial.println("Hand detected but DOOR LOCKED");
    }
  } else {
    if (doorCurrentlyOpen && !isManualLockEnabled) {
      doorClose();
    }
  }

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
  if (!doorCurrentlyOpen) {
    myServo.attach(SERVO_PIN);
    myServo.write(90);
    delay(4000);
    doorCurrentlyOpen = true;
    Blynk.virtualWrite(VIRTUAL_PIN_DOOR_STATUS, 1);
    myServo.detach();
  }
}

void doorClose() {
  if (doorCurrentlyOpen) {
    myServo.attach(SERVO_PIN);
    myServo.write(0);
    delay(4000);
    doorCurrentlyOpen = false;
    Blynk.virtualWrite(VIRTUAL_PIN_DOOR_STATUS, 0);
    myServo.detach();
  }
}