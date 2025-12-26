#include "Door_control.h"
#include "pin_config.h"
#include "BLEModule.h"
#include <Arduino.h>

extern BLEModule ble;

DoorControl::DoorControl()
  : locked(false), pulseActive(false), pulsePin(-1), pulseEnd(0), pesawatOn(false), pesawatStateHigh(false), pesawatNextToggle(0), hazardActive(false), hazardLockedMode(false), hazardStep(0), hazardNext(0), hazardAlarmMode(false), alarmOn(false), alarmStateHigh(false), alarmNextToggle(0) {}

void DoorControl::begin() {
  pinMode(PIN_LOCK, OUTPUT);
  pinMode(PIN_UNLOCK, OUTPUT);
  pinMode(PIN_HAZZARD, OUTPUT);
  pinMode(PIN_PESAWAT, OUTPUT);
  digitalWrite(PIN_LOCK, LOW);
  digitalWrite(PIN_UNLOCK, LOW);
  digitalWrite(PIN_HAZZARD, LOW);
  digitalWrite(PIN_PESAWAT, LOW);
}

void DoorControl::lockPulse() {
  if (locked || pulseActive) {
    ble.notify(std::string("IGNORED LOCK"));
    Serial.println("Ignored LOCK (already locked or busy)");
  } else {
    digitalWrite(PIN_UNLOCK, LOW);
    digitalWrite(PIN_LOCK, HIGH);
    pulseActive = true;
    pulsePin = PIN_LOCK;
    pulseEnd = millis() + 600;
    ble.notify(std::string("LOCK"));
    Serial.println("Action: LOCK started (600ms pulse)");
  }
}

void DoorControl::unlockPulse() {
  if (!locked || pulseActive) {
    ble.notify(std::string("IGNORED UNLOCK"));
    Serial.println("Ignored UNLOCK (already unlocked or busy)");
  } else {
    digitalWrite(PIN_LOCK, LOW);
    digitalWrite(PIN_UNLOCK, HIGH);
    pulseActive = true;
    pulsePin = PIN_UNLOCK;
    pulseEnd = millis() + 600;
    ble.notify(std::string("UNLOCK"));
    Serial.println("Action: UNLOCK started (600ms pulse)");
  }
}

void DoorControl::toggleAlarm() {
  setAlarm(!alarmOn);
}

void DoorControl::setAlarm(bool on) {
  alarmOn = on;
  if (on) {
    hazardAlarmMode = true;
    hazardActive = true;
    hazardStep = 0;
    hazardNext = millis();
    ble.notify(std::string("ALARM ON"));
    Serial.println("Action: ALARM ON");
  } else {
    hazardAlarmMode = false;
    hazardActive = false;
    digitalWrite(PIN_HAZZARD, LOW);
    ble.notify(std::string("ALARM OFF"));
    Serial.println("Action: ALARM OFF");
  }
}

void DoorControl::update() {
  // Complete any active pulse and set locked/unlocked state
  if (pulseActive && millis() >= pulseEnd) {
    digitalWrite(pulsePin, LOW);
    pulseActive = false;
    if (pulsePin == PIN_LOCK) {
      locked = true;
      Serial.println("Locked: true");
      ble.notify(std::string("LOCKED"));
      hazardActive = false;
      hazardLockedMode = true;
      hazardStep = 0;
      hazardNext = millis();
      hazardActive = true;
      pesawatOn = true;
      pesawatStateHigh = true;
      digitalWrite(PIN_PESAWAT, HIGH);
      pesawatNextToggle = millis() + 300;
    } else if (pulsePin == PIN_UNLOCK) {
      locked = false;
      Serial.println("Locked: false (unlocked)");
      ble.notify(std::string("UNLOCKED"));
      hazardActive = false;
      hazardLockedMode = false;
      hazardStep = 0;
      hazardNext = millis();
      hazardActive = true;
      pesawatOn = false;
      pesawatStateHigh = false;
      digitalWrite(PIN_PESAWAT, LOW);
    }
    pulsePin = -1;
  }

  // Alarm blinking (non-blocking)
  if (alarmOn && millis() >= alarmNextToggle) {
    if (alarmStateHigh) {
      digitalWrite(PIN_ALARM, LOW);
      alarmStateHigh = false;
      alarmNextToggle = millis() + 200;
    } else {
      digitalWrite(PIN_ALARM, HIGH);
      alarmStateHigh = true;
      alarmNextToggle = millis() + 200;
    }
  }

  // Pesawat blinking (while pesawatOn)
  if (pesawatOn && millis() >= pesawatNextToggle) {
    if (pesawatStateHigh) {
      digitalWrite(PIN_PESAWAT, LOW);
      pesawatStateHigh = false;
      pesawatNextToggle = millis() + 3000;
    } else {
      digitalWrite(PIN_PESAWAT, HIGH);
      pesawatStateHigh = true;
      pesawatNextToggle = millis() + 100;
    }
  }

  // Hazard light control (non-blocking state machine)
  if (hazardActive && millis() >= hazardNext) {
    if (hazardAlarmMode) {
      if (hazardStep == 0) {
        digitalWrite(PIN_HAZZARD, HIGH);
        hazardStep = 1;
        hazardNext = millis() + 200;
      } else {
        digitalWrite(PIN_HAZZARD, LOW);
        hazardStep = 0;
        hazardNext = millis() + 200;
      }
    } else if (hazardLockedMode) {
      switch (hazardStep) {
        case 0:
          digitalWrite(PIN_HAZZARD, HIGH);
          hazardNext = millis() + 400;
          hazardStep++;
          break;
        case 1:
          digitalWrite(PIN_HAZZARD, LOW);
          hazardNext = millis() + 200;
          hazardStep++;
          break;
        case 2:
          digitalWrite(PIN_HAZZARD, HIGH);
          hazardNext = millis() + 400;
          hazardStep++;
          break;
        default:
          digitalWrite(PIN_HAZZARD, LOW);
          hazardActive = false;
          break;
      }
    } else {
      if (hazardStep == 0) {
        digitalWrite(PIN_HAZZARD, HIGH);
        hazardNext = millis() + 1000;
        hazardStep++;
      } else {
        digitalWrite(PIN_HAZZARD, LOW);
        hazardActive = false;
      }
    }
  }
}

void DoorControl::cancelAll() {
  pulseActive = false;
  pulsePin = -1;
  hazardActive = false;
  hazardAlarmMode = false;
  alarmOn = false;
  digitalWrite(PIN_HAZZARD, LOW);
  digitalWrite(PIN_PESAWAT, LOW);
}

bool DoorControl::isLocked() const { return locked; }

void DoorControl::setEngineState(bool on) {
  if (on) {
    pesawatOn = false;
    pesawatStateHigh = false;
    digitalWrite(PIN_PESAWAT, LOW);
  } else {
    if (locked) {
      pesawatOn = true;
      pesawatStateHigh = true;
      digitalWrite(PIN_PESAWAT, HIGH);
      pesawatNextToggle = millis() + 300;
    } else {
      pesawatOn = false;
      pesawatStateHigh = false;
      digitalWrite(PIN_PESAWAT, LOW);
    }
  }
}
