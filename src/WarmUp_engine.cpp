#include "WarmUp_engine.h"
#include "pin_config.h"
#include "BLEModule.h"
#include <Preferences.h>
#include <Arduino.h>

extern BLEModule ble;
// starterActive and starterEnd are defined in main.cpp and used to manage starter pulse
extern bool starterActive;
extern unsigned long starterEnd;

WarmUpEngine::WarmUpEngine()
  : _rtc(nullptr), _prefs(nullptr), _engineSetter(nullptr), warmActive(false), warmEnd(0), warmStarterPending(false), warmStarterAt(0), lastWarmDay(-1), warmDurationMinutes(10) {}

void WarmUpEngine::init(RTCModule* rtc, Preferences* prefs, std::function<void(bool)> engineSetter) {
  _rtc = rtc;
  _prefs = prefs;
  _engineSetter = engineSetter;
}

void WarmUpEngine::begin() {
  if (_prefs) {
    warmDurationMinutes = _prefs->getInt("warmlen", warmDurationMinutes);
    Serial.printf("Warm duration loaded: %d minutes\n", warmDurationMinutes);
  }
}

bool WarmUpEngine::rtcTimePlausible() const {
  if (!_rtc) return false;
  DateTime t = _rtc->now();
  int y = t.year();
  if (y < 2020 || y > 2035) return false;
  if (_rtc->lostPowerFlag()) return false;
  return true;
}

DateTime WarmUpEngine::rtcSafeNow() const {
  if (!_rtc) return DateTime(2000,1,1,0,0,0);
  if (rtcTimePlausible()) return _rtc->now();
  return DateTime(2000,1,1,0,0,0);
}

void WarmUpEngine::update() {
  // Execute warm starter pending
  if (warmStarterPending && millis() >= warmStarterAt) {
    warmStarterPending = false;
    if (!starterActive) {
      digitalWrite(PIN_STARTER, HIGH);
      starterActive = true;
      starterEnd = millis() + 1000;
      if (_engineSetter) _engineSetter(true);
      Serial.println("Warm-up: STARTER pulse started (1s)");
      ble.notify(std::string("STARTER ON"));
    }
  }

  // Finish warm period
  if (warmActive && millis() >= warmEnd) {
    warmActive = false;
    digitalWrite(PIN_ACC, LOW);
    digitalWrite(PIN_IG, LOW);
    digitalWrite(PIN_STARTER, LOW);
    starterActive = false;
    if (_engineSetter) _engineSetter(false);
    digitalWrite(PIN_LAMP, LOW);
    digitalWrite(PIN_ALARM, LOW);
    ble.notify(std::string("WARM DONE"));
    Serial.println("Warm-up complete: systems turned off (pesawat unaffected)");
  }

  // Daily warm-up trigger
  DateTime _dt = rtcSafeNow();
  if (rtcTimePlausible()) {
    int hour = _dt.hour();
    int minute = _dt.minute();
    int day = _dt.day();
    if (hour == 15 && minute == 31 && lastWarmDay != day) {
      lastWarmDay = day;
      warmActive = true;
      warmEnd = millis() + ((unsigned long)warmDurationMinutes * 60UL * 1000UL);
      digitalWrite(PIN_IG, HIGH);
      // schedule starter after 1000ms
      warmStarterPending = true;
      warmStarterAt = millis() + 1000;
      ble.notify(std::string("WARM ON"));
      Serial.printf("Warm-up scheduled: IG_ON, starter in 1s, duration %d min\n", warmDurationMinutes);
    }
  }
}

void WarmUpEngine::forceWarm() {
  if (!warmActive) {
    warmActive = true;
    warmEnd = millis() + ((unsigned long)warmDurationMinutes * 60UL * 1000UL);
    digitalWrite(PIN_IG, HIGH);
    warmStarterPending = true;
    warmStarterAt = millis() + 1000;
    Serial.printf("Warm-up forced: IG_ON, starter in 1s, duration %d min\n", warmDurationMinutes);
    ble.notify(std::string("WARM ON"));
  } else {
    Serial.println("Warm-up already active");
  }
}

void WarmUpEngine::cancelWarm() {
  warmActive = false;
  warmStarterPending = false;
}

void WarmUpEngine::setDurationMinutes(int m) {
  if (m >= 1 && m <= 60) {
    warmDurationMinutes = m;
    if (_prefs) _prefs->putInt("warmlen", warmDurationMinutes);
    Serial.printf("Warm duration set to %d minutes (saved)\n", warmDurationMinutes);
  } else {
    Serial.println("Invalid minutes (1-60)");
  }
}

int WarmUpEngine::getDurationMinutes() const { return warmDurationMinutes; }
bool WarmUpEngine::isActive() const { return warmActive; }
unsigned long WarmUpEngine::remainingMillis() const { if (!warmActive) return 0; if (warmEnd > millis()) return warmEnd - millis(); return 0; }

// init is implemented above
