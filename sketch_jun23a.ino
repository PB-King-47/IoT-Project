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

char ssid[] ="Bdconnect";
char pass[] ="01762603";

// Global variables
int distance;
int level;
int fullBinCount = 0;
bool isDoorLocked = false;
bool isManualLockEnabled = false;  // New: separate manual lock control
bool doorCurrentlyOpen = false;
unsigned long doorOpenTime = 0;

// Pin definitions
#define TRIG1 4       // Bin level sensor
#define ECHO1 16
#define TRIG2 17      // Hand detection sensor
#define ECHO2 5
#define LED_PIN 2
#define BUZZER_PIN 22
#define SERVO_PIN 21

// Configuration constants
#define BIN_FULL_THRESHOLD 5.0    // cm - when bin is considered full
#define MAX_DISTANCE 11.0         // cm - maximum measurable distance
#define MIN_DISTANCE 5.0          // cm - minimum measurable distance
#define HAND_DETECT_DISTANCE 30   // cm - hand detection range
#define DOOR_OPEN_DURATION 3000   // ms - how long to keep door open
#define FULL_BIN_CONFIRM_COUNT 5  // confirmations needed before locking

// Virtual pins
#define VIRTUAL_PIN_LEVEL V0
#define VIRTUAL_PIN_BIN_STATUS V1
#define VIRTUAL_PIN_DOOR_SWITCH V2
#define VIRTUAL_PIN_DOOR_STATUS V3    // New: shows door open/closed status

// Initialize HCSR04 sensors
HCSR04 binLevelSensor(TRIG1, ECHO1);     // Sensor 1: Bin level detection
HCSR04 handDetectSensor(TRIG2, ECHO2);   // Sensor 2: Hand detection

// Function prototypes
int calculateLevel(float dist, int virtualPin);
void doorOpen();
void doorClose();
void binAlarm();
void updateDoorStatus();

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize servo
  myServo.attach(SERVO_PIN); 
  myServo.write(0);  // Start with door closed
  
  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // Set up timer
  timer.setInterval(500L, binMain);  // Reduced interval for better responsiveness
  
  Serial.println("Smart Trash Bin system ready");
  
  // Initialize Blynk virtual pins
  Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, LOW);
  Blynk.virtualWrite(VIRTUAL_PIN_DOOR_SWITCH, 0);
  Blynk.virtualWrite(VIRTUAL_PIN_DOOR_STATUS, 0);
}

void loop() {
  Blynk.run();
  timer.run();
  
  // Auto-close door after timeout
  if (doorCurrentlyOpen && (millis() - doorOpenTime > DOOR_OPEN_DURATION)) {
    if (!isManualLockEnabled && !isDoorLocked) {
      doorClose();
    }
  }
}

// Blynk App Door Switch Control
BLYNK_WRITE(VIRTUAL_PIN_DOOR_SWITCH) {
  int switchState = param.asInt();
  isManualLockEnabled = (switchState == 1);
  
  if (isManualLockEnabled) {
    Serial.println("Manual LOCK enabled - Door will stay closed");
    doorClose();  // Close door when manually locked
  } else {
    Serial.println("Manual UNLOCK enabled - Normal operation resumed");
    fullBinCount = 0;  // Reset full bin counter when manually unlocked
    isDoorLocked = false;  // Clear auto-lock when manually unlocked
  }
}

// Main Trash Bin Function
void binMain() {  
  // Read sensors using HCSR04 library
  float binLevelDistance = binLevelSensor.dist();
  float handDistance = handDetectSensor.dist();
  
  // Calculate and update bin level
  level = calculateLevel(binLevelDistance, VIRTUAL_PIN_LEVEL);
  
  // Debug output
  Serial.print("Bin Level: ");
  Serial.print(binLevelDistance);
  Serial.print(" cm → ");
  Serial.print(level);
  Serial.print("% | Hand: ");
  Serial.print(handDistance);
  Serial.print(" cm | Status: ");
  
  if (isManualLockEnabled) {
    Serial.println("MANUAL LOCK");
  } else if (isDoorLocked) {
    Serial.println("AUTO LOCKED (FULL)");
  } else {
    Serial.println(doorCurrentlyOpen ? "DOOR OPEN" : "NORMAL");
  }

  // Handle bin full condition
  if (binLevelDistance <= BIN_FULL_THRESHOLD) {
    Serial.println("Trash Bin is full!");
    binAlarm();
    digitalWrite(LED_PIN, HIGH);
    
    fullBinCount++;
    
    // Lock door after multiple confirmations
    if (fullBinCount >= FULL_BIN_CONFIRM_COUNT && !isDoorLocked && !isManualLockEnabled) {
      Serial.println("Bin FULL confirmed, auto-locking...");
      isDoorLocked = true;
      doorClose();
      
      // Update Blynk - but don't change manual switch
      Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, HIGH);
      
      // Optional: Send notification
      // Blynk.logEvent("bin_full", "Trash Bin is full and auto-locked!");
    }
  } else {
    // Bin not full
    fullBinCount = 0;
    digitalWrite(LED_PIN, LOW);
    Blynk.virtualWrite(VIRTUAL_PIN_BIN_STATUS, LOW);
  }

  // Handle door control based on different lock states
  if (!isManualLockEnabled) {
    // Manual lock is OFF - door can operate normally
    if (handDistance <= HAND_DETECT_DISTANCE && handDistance > 0) {
      // Hand detected
      if (isDoorLocked) {
        // Bin is full but manual lock is off - allow one-time opening with warning
        Serial.println("WARNING: Bin is full but hand detected - Opening door briefly");
        if (!doorCurrentlyOpen) {
          doorOpen();
        }
      } else {
        // Normal operation - open door
        if (!doorCurrentlyOpen) {
          Serial.println("Hand detected - Opening door...");
          doorOpen();
        }
      }
    }
    // Door will auto-close after timeout (handled in main loop)
  } else {
    // Manual lock is ON - door stays closed regardless of hand detection
    if (handDistance <= HAND_DETECT_DISTANCE && handDistance > 0) {
      Serial.println("Hand detected but door is MANUALLY LOCKED - Door remains closed");
    }
    if (doorCurrentlyOpen) {
      doorClose(); // Force close if somehow open
    }
  }

  Serial.println("----------");
}
// --- Level Calculation ---
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

// Door control functions
void doorOpen() {
  if (!doorCurrentlyOpen) {
    myServo.write(90);
    doorCurrentlyOpen = true;
    doorOpenTime = millis();
    updateDoorStatus();
    Serial.println("Door opened");
  }
}

void doorClose() {
  if (doorCurrentlyOpen) {
    myServo.write(0);
    doorCurrentlyOpen = false;
    updateDoorStatus();
    Serial.println("Door closed");
  }
}

// Update door status on Blynk app
void updateDoorStatus() {
  Blynk.virtualWrite(VIRTUAL_PIN_DOOR_STATUS, doorCurrentlyOpen ? 1 : 0);
}