#include "ButtonTombol.h"
#include "pin_config.h"

ButtonTombol::ButtonTombol(uint8_t buttonPin, uint8_t ledPin)
  : _btnPin(buttonPin), _ledPin(ledPin), _lastState(LOW), _stateUntil(0), _countdownUntil(0),
    _state(IDLE), _starterRunning(false), _engineOn(false), _onReset(nullptr), _setEngine(nullptr),
    _ledToggleAt(0), _ledHigh(false), _ledHighMs(500), _ledLowMs(500) {
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
    // stable state
    if (v != _lastState) {
      // detect falling edge (HIGH -> LOW) as press
      if (_lastState == HIGH && v == LOW) {
        // If engine already on, pressing triggers reset
        if (_engineOn) {
          if (_onReset) _onReset();
        } else {
          // First press: start sequence and start countdown window
          if (_state == IDLE) {
            digitalWrite(PIN_ACC, HIGH);
            if (_setEngine) _setEngine(false);
            _state = ACC_WAIT;
            _stateUntil = now + 1000UL;
            // start overall countdown now (5s from first press)
            _countdownUntil = now + 5000UL;
            // keep LED slow until starter
            setLedBlink(500,500);
          } else if (_state == COUNTDOWN) {
            // during countdown, pressing restarts starter attempt immediately
            // start starter now
            digitalWrite(PIN_STARTER, HIGH);
            _starterRunning = true;
            _state = STARTER_ACTIVE;
            _stateUntil = now + 1000UL; // starter duration
            // reset countdown window
            _countdownUntil = now + 5000UL;
            // fast blink while starter running
            setLedBlink(200,50);
          }
          // presses during ACC_WAIT/IG_WAIT/STARTER_ACTIVE ignored
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
        _stateUntil = now + 2000UL;
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
