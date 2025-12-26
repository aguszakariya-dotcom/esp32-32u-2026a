#include "ButtonTombol.h"
#include "pin_config.h"

ButtonTombol::ButtonTombol(uint8_t buttonPin, uint8_t ledPin)
  : _btnPin(buttonPin), _ledPin(ledPin), _lastState(LOW), _stateUntil(0), _countdownUntil(0),
    _state(IDLE), _starterRunning(false), _engineOn(false), _onReset(nullptr), _setEngine(nullptr),
    _ledToggleAt(0), _ledHigh(false), _ledHighMs(500), _ledLowMs(500), _countdownMs(15000UL), _manualStarterHold(false) {
}

void ButtonTombol::begin() {
  // Use internal pull-up; wire button to GND
  pinMode(_btnPin, INPUT_PULLUP);
  pinMode(_ledPin, OUTPUT);
  digitalWrite(_ledPin, LOW);
  _lastRead = digitalRead(_btnPin);
  _lastState = _lastRead;
  _lastDebounceTime = millis();
  // start idle slow blink
  _ledHighMs = 500; _ledLowMs = 500;
  _ledHigh = false;
  _ledToggleAt = millis() + _ledLowMs;
}

void ButtonTombol::setLedBlink(unsigned long highMs, unsigned long lowMs) {
  _ledHighMs = highMs; _ledLowMs = lowMs;
}

void ButtonTombol::triggerStart() {
  unsigned long now = millis();
  // If engine already on, behave like press -> reset
  if (_engineOn) {
    if (_onReset) _onReset();
    return;
  }
  if (_state == IDLE) {
    digitalWrite(PIN_ACC, HIGH);
    if (_setEngine) _setEngine(false);
    _state = ACC_WAIT;
    _stateUntil = now + 1000UL;
    _countdownUntil = now + _countdownMs;
    setLedBlink(500,500);
  } else if (_state == COUNTDOWN) {
    // restart starter attempt immediately
    digitalWrite(PIN_STARTER, HIGH);
    _starterRunning = true;
    _state = STARTER_ACTIVE;
    _stateUntil = now + 1000UL;
    _countdownUntil = now + _countdownMs;
    setLedBlink(200,50);
  }
}

void ButtonTombol::update() {
  unsigned long now = millis();
  int v = digitalRead(_btnPin);

  // LED blinking handler (non-blocking)
  if (now >= _ledToggleAt) {
    _ledHigh = !_ledHigh;
    digitalWrite(_ledPin, _ledHigh ? HIGH : LOW);
    _ledToggleAt = now + (_ledHigh ? _ledHighMs : _ledLowMs);
  }

  // Debounce logic (INPUT_PULLUP): pressed when reading == LOW
  if (v != _lastRead) {
    _lastDebounceTime = now;
    _lastRead = v;
  }
  if (now - _lastDebounceTime > _debounceMs) {
    // stable state changed
    if (v != _lastState) {
      // falling edge (press)
      if (_lastState == HIGH && v == LOW) {
        if (_engineOn) {
          if (_onReset) _onReset();
        } else {
          if (_state == IDLE) {
            digitalWrite(PIN_ACC, HIGH);
            if (_setEngine) _setEngine(false);
            _state = ACC_WAIT;
            _stateUntil = now + 1000UL;
            // start overall countdown now
            _countdownUntil = now + _countdownMs;
            setLedBlink(500,500);
          } else if (_state == COUNTDOWN) {
            // enter manual hold mode: starter on while held
            digitalWrite(PIN_STARTER, HIGH);
            _manualStarterHold = true;
            _starterRunning = true;
            // reset countdown window
            _countdownUntil = now + _countdownMs;
            setLedBlink(200,50);
          }
        }
      }
      // rising edge (release)
      else if (_lastState == LOW && v == HIGH) {
        // if manual hold was active, release starter
        if (_manualStarterHold && _state == COUNTDOWN) {
          digitalWrite(PIN_STARTER, LOW);
          _manualStarterHold = false;
          _starterRunning = false;
          // keep countdown running, LED remain fast
          setLedBlink(200,50);
        }
      }
      _lastState = v;
    }
  }

  // State machine timing
  switch (_state) {
    case IDLE:
      // nothing else; idle blink already handled
      break;
    case ACC_WAIT:
      if (now >= _stateUntil) {
        // turn on IG, wait 2s then starter
        digitalWrite(PIN_IG, HIGH);
        _state = IG_WAIT;
        _stateUntil = now + 1000UL;
      }
      break;
    case IG_WAIT:
      if (now >= _stateUntil) {
        // start starter pulse
        digitalWrite(PIN_STARTER, HIGH);
        _starterRunning = true;
        _state = STARTER_ACTIVE;
        _stateUntil = now + 1000UL; // starter pulse 1s
        // fast blink LED
        setLedBlink(200,50);
      }
      break;
    case STARTER_ACTIVE:
      if (now >= _stateUntil) {
        digitalWrite(PIN_STARTER, LOW);
        _starterRunning = false;
        // go to COUNTDOWN state; countdown was started on first press and may already be running
        _state = COUNTDOWN;
        // during countdown, fast blink
        setLedBlink(200,50);
      }
      break;
    case COUNTDOWN:
      if (now >= _countdownUntil) {
        // assume engine started
        if (_setEngine) _setEngine(true);
        _engineOn = true;
        // back to idle slow blink
        _state = IDLE;
        setLedBlink(500,500);
      }
      break;
  }
}

void ButtonTombol::setEngineStatus(bool v) {
  _engineOn = v;
}
