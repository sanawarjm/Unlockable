#include <Wire.h>
#include "DFRobot_RGBLCD1602.h"
#include <ESP32Servo.h>

// ================= LCD ==================
DFRobot_RGBLCD1602 lcd(0x6B, 16, 2);

// ================= SERVO =================
Servo lockServo;
const int servoPin = 26;
const int lockedPos = 0;
const int unlockedPos = 90;

// ================= BUTTONS =================
const int PB1 = 18;
const int PB2 = 19;
const int PB3 = 23;
const int PB4 = 25;
const int PB5 = 33;
const int PB6 = 32;

// ================= LEDs =================
const int led1 = 4;
const int led2 = 5;
const int led3 = 16;

// ================= BUZZER =================
const int buzzerPin = 17;

// ================= PASSWORD =================
const int passwordLength = 4;
int correctPassword[passwordLength] = {1,2,3,4};
int enteredPassword[passwordLength];
int digitIndex = 0;

// ================= ATTEMPTS =================
int attemptCount = 0;
const int maxAttempts = 3;

// ================= LOCKOUT =================
bool isTimedLockout = false;
unsigned long lockoutStart = 0;
const unsigned long lockoutDuration = 30000;

// ================= STATE FLAGS =================
bool isUnlocked = false;
bool isChangingPassword = false;
bool verifyingOldPassword = false;

// ================= DEBOUNCE =================
unsigned long lastButtonTime = 0;
const unsigned long debounceDelay = 250;

// ============================================================

void setup() {

  Serial.begin(115200);

  pinMode(PB1, INPUT_PULLUP);
  pinMode(PB2, INPUT_PULLUP);
  pinMode(PB3, INPUT_PULLUP);
  pinMode(PB4, INPUT_PULLUP);
  pinMode(PB5, INPUT_PULLUP);
  pinMode(PB6, INPUT_PULLUP);

  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  resetLEDs();

  lockServo.setPeriodHertz(50);
  lockServo.attach(servoPin, 500, 2400);
  lockServo.write(lockedPos);

  Wire.begin(21, 22);
  lcd.init();
  lcd.clear();

  showDefaultPassword();
  delay(2000);

  showEnterScreen();
}

// ============================================================

void loop() {

  // LOCKOUT
  if (isTimedLockout) {
    handleTimedLockout();
    return;
  }

  // UNLOCKED STATE
  if (isUnlocked && !isChangingPassword) {

    if (digitalRead(PB1) == LOW) {
      while (digitalRead(PB1) == LOW);
      relockSystem();
      return;
    }

    if (digitalRead(PB6) == LOW) {
      while (digitalRead(PB6) == LOW);
      startPasswordChange();
      return;
    }

    return;
  }

  int pressed = checkButtons();

  if (pressed != 0) {

    enteredPassword[digitIndex] = pressed;
    digitIndex++;

    lcd.setCursor(digitIndex - 1, 1);
    lcd.print("*");

    if (digitIndex >= passwordLength) {

      if (isChangingPassword) {
        handlePasswordChange();
      } else {
        checkPassword();
      }

      digitIndex = 0;
    }
  }
}

// ============================================================

void showDefaultPassword() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Default Pass:");
  lcd.setCursor(0,1);
  lcd.print("1234");
}

void showEnterScreen() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Enter Password:");
  lcd.setCursor(0,1);
}

// ============================================================

void startPasswordChange() {

  isChangingPassword = true;
  verifyingOldPassword = true;
  digitIndex = 0;

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Confirm Old Pass");
  lcd.setCursor(0,1);
}

// ============================================================

void handlePasswordChange() {

  if (verifyingOldPassword) {

    if (isPasswordCorrect()) {

      verifyingOldPassword = false;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Enter New Pass");
      lcd.setCursor(0,1);

    } else {

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Wrong Old Pass");
      delay(1500);

      isChangingPassword = false;
      verifyingOldPassword = false;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ACCESS GRANTED");
      lcd.setCursor(0,1);
      lcd.print("Press PB1 lock");
    }

  } else {

    for (int i = 0; i < passwordLength; i++) {
      correctPassword[i] = enteredPassword[i];
    }

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Password Updated");
    delay(1500);

    isChangingPassword = false;

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ACCESS GRANTED");
    lcd.setCursor(0,1);
    lcd.print("Press PB1 lock");
  }
}

// ============================================================

void checkPassword() {

  if (isPasswordCorrect()) {
    unlockDoor();
  } else {
    wrongAttempt();
  }
}

// ============================================================

bool isPasswordCorrect() {

  for (int i = 0; i < passwordLength; i++) {
    if (enteredPassword[i] != correctPassword[i])
      return false;
  }
  return true;
}

// ============================================================

void unlockDoor() {

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("ACCESS GRANTED");
  lcd.setCursor(0,1);
  lcd.print("Press PB1 lock");

  tone(buzzerPin, 3000, 200);

  lockServo.write(unlockedPos);

  attemptCount = 0;
  resetLEDs();
  isUnlocked = true;
}

// ============================================================

void relockSystem() {

  lockServo.write(lockedPos);

  isUnlocked = false;
  digitIndex = 0;
  attemptCount = 0;
  resetLEDs();

  showEnterScreen();
}

// ============================================================

void wrongAttempt() {

  attemptCount++;

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WRONG PASSWORD");

  tone(buzzerPin, 2000, 200);

  if (attemptCount == 1) digitalWrite(led3, LOW);
  if (attemptCount == 2) digitalWrite(led2, LOW);
  if (attemptCount == 3) digitalWrite(led1, LOW);

  delay(1000);

  if (attemptCount >= maxAttempts) {
    startTimedLockout();
  } else {
    showEnterScreen();
  }
}

// ============================================================

void startTimedLockout() {
  isTimedLockout = true;
  lockoutStart = millis();
}

// ============================================================

void handleTimedLockout() {

  unsigned long elapsed = millis() - lockoutStart;
  unsigned long remaining = (lockoutDuration - elapsed) / 1000;

  lcd.setCursor(0,0);
  lcd.print("SYSTEM LOCKED ");
  lcd.setCursor(0,1);
  lcd.print("Time: ");
  lcd.print(remaining);
  lcd.print("s   ");

  if (elapsed >= lockoutDuration) {
    isTimedLockout = false;
    attemptCount = 0;
    resetLEDs();
    showEnterScreen();
  }
}

// ============================================================

int checkButtons() {

  if (millis() - lastButtonTime < debounceDelay)
    return 0;

  if (digitalRead(PB1) == LOW) return registerPress(1);
  if (digitalRead(PB2) == LOW) return registerPress(2);
  if (digitalRead(PB3) == LOW) return registerPress(3);
  if (digitalRead(PB4) == LOW) return registerPress(4);
  if (digitalRead(PB5) == LOW) return registerPress(5);

  // PB6 acts as digit ONLY if not unlocked
  if (!isUnlocked && digitalRead(PB6) == LOW) return registerPress(6);

  return 0;
}

int registerPress(int value) {

  lastButtonTime = millis();
  while (digitalRead(getPinFromValue(value)) == LOW);
  return value;
}

int getPinFromValue(int value) {

  switch(value) {
    case 1: return PB1;
    case 2: return PB2;
    case 3: return PB3;
    case 4: return PB4;
    case 5: return PB5;
    case 6: return PB6;
  }
  return 0;
}

// ============================================================

void resetLEDs() {
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);
}