#include "RX500Module.h"

RX500Module::RX500Module(uint8_t pinA, uint8_t pinB, uint8_t pinC, uint8_t pinD)
  : _pinA(pinA), _pinB(pinB), _pinC(pinC), _pinD(pinD),
    _lastA(LOW), _lastB(LOW), _lastC(LOW), _lastD(LOW),
    _onLock(nullptr), _onUnlock(nullptr), _onStart(nullptr), _onAlarmToggle(nullptr) {
}

void RX500Module::begin() {
  pinMode(_pinA, INPUT);
  pinMode(_pinB, INPUT);
  pinMode(_pinC, INPUT);
  pinMode(_pinD, INPUT);
  // read initial state
  _lastA = digitalRead(_pinA);
  _lastB = digitalRead(_pinB);
  _lastC = digitalRead(_pinC);
  _lastD = digitalRead(_pinD);
}

void RX500Module::update() {
  int a = digitalRead(_pinA);
  int b = digitalRead(_pinB);
  int c = digitalRead(_pinC);
  int d = digitalRead(_pinD);

  // rising edges
  if (a == HIGH && _lastA == LOW) {
    if (_onLock) _onLock();
  }
  _lastA = a;

  if (b == HIGH && _lastB == LOW) {
    if (_onUnlock) _onUnlock();
  }
  _lastB = b;

  if (c == HIGH && _lastC == LOW) {
    if (_onStart) _onStart();
  }
  _lastC = c;

  if (d == HIGH && _lastD == LOW) {
    if (_onAlarmToggle) _onAlarmToggle();
  }
  _lastD = d;
}

void RX500Module::setOnLock(std::function<void()> cb) { _onLock = cb; }
void RX500Module::setOnUnlock(std::function<void()> cb) { _onUnlock = cb; }
void RX500Module::setOnStart(std::function<void()> cb) { _onStart = cb; }
void RX500Module::setOnAlarmToggle(std::function<void()> cb) { _onAlarmToggle = cb; }
