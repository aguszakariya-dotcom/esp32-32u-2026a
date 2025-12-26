#pragma once
#include <Arduino.h>
#include <functional>

class ButtonTombol {
public:
  ButtonTombol(uint8_t buttonPin = 18, uint8_t ledPin = 22);
  void begin();
  void update();
  void setResetCallback(std::function<void()> cb) { _onReset = cb; }
  void setEngineSetter(std::function<void(bool)> cb) { _setEngine = cb; }
  void setEngineStatus(bool v);
  // Set countdown window after starter attempt (ms). Default 15000 (15s)
  void setCountdownMs(unsigned long ms) { _countdownMs = ms; }
  // Trigger start sequence externally (e.g. from BLE/remote). Behaves like initial button press.
  void triggerStart();
private:
  enum State { IDLE, ACC_WAIT, IG_WAIT, STARTER_ACTIVE, COUNTDOWN };
  uint8_t _btnPin, _ledPin;
  int _lastState;
  int _lastRead;
  // debounce
  unsigned long _lastDebounceTime;
  const unsigned long _debounceMs = 50;
  unsigned long _stateUntil;
  unsigned long _countdownUntil;
  unsigned long _countdownMs;
  State _state;
  bool _starterRunning;
  bool _manualStarterHold;
  bool _engineOn;
  std::function<void()> _onReset;
  std::function<void(bool)> _setEngine;
  
  // LED blink helper
  unsigned long _ledToggleAt;
  bool _ledHigh;
  void setLedBlink(unsigned long highMs, unsigned long lowMs);
  unsigned long _ledHighMs, _ledLowMs;
};
