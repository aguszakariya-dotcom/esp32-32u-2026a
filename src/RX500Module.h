#pragma once
#include <Arduino.h>
#include <functional>
#include "pin_config.h"

class RX500Module {
public:
  RX500Module(uint8_t pinA = LEDIN_A, uint8_t pinB = LEDIN_B, uint8_t pinC = LEDIN_C, uint8_t pinD = LEDIN_D);
  void begin();
  void update();
  void setOnLock(std::function<void()> cb);
  void setOnUnlock(std::function<void()> cb);
  void setOnStart(std::function<void()> cb);
  void setOnAlarmToggle(std::function<void()> cb);

private:
  uint8_t _pinA, _pinB, _pinC, _pinD;
  int _lastA, _lastB, _lastC, _lastD;
  std::function<void()> _onLock;
  std::function<void()> _onUnlock;
  std::function<void()> _onStart;
  std::function<void()> _onAlarmToggle;
};
