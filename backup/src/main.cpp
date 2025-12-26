#include <Arduino.h>

// Simple BLE peripheral example for ESP32
// Prints: device address (MAC), service UUID, characteristic UUID
// Use these values in MIT App Inventor's BluetoothLE extension

#include "BLEModule.h"
#include "RTCModule.h"
#include "pin_config.h"
#include "RX500Module.h"
#include "ButtonTombol.h"
#include <Wire.h>
#include <Preferences.h>
#include <algorithm>
#include <cctype>

// Change these UUIDs as needed (use full 128-bit UUIDs for App Inventor)
static BLEUUID SERVICE_UUID("12345678-1234-1234-1234-123456789abc");
static BLEUUID CHARACTERISTIC_UUID("abcdefab-1234-5678-1234-abcdefabcdef");

// Serial baud: default 115200. You can change at runtime by typing `b9600` or
// `b115200` on the Serial Console within the first 3 seconds after reset.
static const uint32_t DEFAULT_BAUD = 115200;
static uint32_t serialBaud = DEFAULT_BAUD;

// LED pin turned on when a BLE central connects. Change if your board uses
// a different built-in LED pin.
// LED pin turned on when a BLE central connects. See include/pin_config.h
// (LED_PIN, PIN_LOCK, PIN_UNLOCK, PIN_HAZZARD are defined there)

// Track connection state explicitly
static bool isConnected = false;
// Lock state and pulse control to operate the solenoid safely
static bool locked = false; // true when lock is engaged
static bool pulseActive = false;
static int pulsePin = -1;
static unsigned long pulseEnd = 0;

// variables for BLE command processing
// ===== Remote input edge detection (latch trigger) =====
static int lastInA = LOW;
static int lastInIg = LOW;
static int lastInStarter = LOW;
static int lastInAlarm = LOW;

// Misc states for other commands
static bool accOn = false;
static bool igOn = false;
static bool engineOn = false; // starter pulse
static bool alarmOn = false;
static bool lampOn = false;

// Starter pulse control (non-blocking)
static bool starterActive = false;
static unsigned long starterEnd = 0;
// Start_the_Car composite command flags
static volatile bool startCarPending = false;
static volatile unsigned long startCarAt = 0;

// Alarm blink control (non-blocking)
static bool alarmStateHigh = false;
static unsigned long alarmNextToggle = 0;
// Pesawat (indicator) control when locked
static bool pesawatOn = false;
static bool pesawatStateHigh = false;
static unsigned long pesawatNextToggle = 0;

// Hazzard & pesawat control (non-blocking)
static bool hazardActive = false;
static bool hazardLockedMode = false; // true=LOCK, false=UNLOCK
static int hazardStep = 0;
static unsigned long hazardNext = 0;
static bool hazardAlarmMode = false; // true = continuous blinking

// Warm-up schedule variables (daily at 08:00)
static bool warmActive = false;
static unsigned long warmEnd = 0;
static bool warmStarterPending = false;
static unsigned long warmStarterAt = 0;
static int lastWarmDay = -1; // day-of-month when warm was last triggered
// Warm duration in minutes (default 10). Can be changed with serial `warmlen` command.
static int warmDurationMinutes = 10;
// Preferences (NVS) to persist settings like warmDurationMinutes
static Preferences prefs;

BLEModule ble;
RTCModule rtc;
RX500Module rx500;
ButtonTombol buttonTombol(18, PIN_LED_POWER); // button pin 18, LED_POWER on PIN_LED_POWER (GPIO13)

// Helper functions for lock/unlock to reuse in BLE and serial handlers
void startLockPulse() {
  if (locked || pulseActive) {
    ble.notify(std::string("IGNORED:LOCK"));
    Serial.println("Ignored LOCK (already locked or busy)");
  } else {
    digitalWrite(PIN_UNLOCK, LOW);
    digitalWrite(PIN_LOCK, HIGH);
    pulseActive = true;
    pulsePin = PIN_LOCK;
    pulseEnd = millis() + 600;
    ble.notify(std::string("ACK:LOCK"));
    Serial.println("Action: LOCK started (600ms pulse)");
  }
}

void startUnlockPulse() {
  if (!locked || pulseActive) {
    ble.notify(std::string("IGNORED:UNLOCK"));
    Serial.println("Ignored UNLOCK (already unlocked or busy)");
  } else {
    digitalWrite(PIN_LOCK, LOW);
    digitalWrite(PIN_UNLOCK, HIGH);
    pulseActive = true;
    pulsePin = PIN_UNLOCK;
    pulseEnd = millis() + 600;
    ble.notify(std::string("ACK:UNLOCK"));
    Serial.println("Action: UNLOCK started (600ms pulse)");
  }
}

// Set engine state and update pesawat indicator accordingly
void setEngineState(bool on) {
  engineOn = on;
  if (on) {
    // When engine is on, turn off pesawat indicator
    pesawatOn = false;
    pesawatStateHigh = false;
    digitalWrite(PIN_PESAWAT, LOW);
    // inform button module
    buttonTombol.setEngineStatus(true);
  } else {
    // When engine turns off, if vehicle is locked show pesawat blinking
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
    // inform button module
    buttonTombol.setEngineStatus(false);
  }
}

// Reset all outputs/state (called from remote or other flows)
void resetAll() {
  // Turn off engine-related outputs and flags
  digitalWrite(PIN_ACC, LOW); accOn = false;
  digitalWrite(PIN_IG, LOW); igOn = false;
  digitalWrite(PIN_ALARM, LOW); alarmOn = false; alarmStateHigh = false;
  digitalWrite(PIN_STARTER, LOW); starterActive = false; setEngineState(false);
  // Optionally add more resets here (e.g., warmActive, pesawat state)
  ble.notify(std::string("ACK:RESET_ALL"));
  Serial.println("Action: RESET_ALL executed (ACC/IG/ALARM/STARTER off)");
}

// Normalize incoming command string to lowercase (preserve underscores)
static std::string normalizeCmd(const std::string &s) {
  std::string r = s;
  // trim leading/trailing spaces
  while (!r.empty() && std::isspace((unsigned char)r.front())) r.erase(r.begin());
  while (!r.empty() && std::isspace((unsigned char)r.back())) r.pop_back();
  std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
  return r;
}

// Pretty-print command for serial output: uppercase and replace '_' with ' '
static String prettyCmd(const std::string &s) {
  std::string r = s;
  std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::toupper(c); });
  for (auto &c : r) if (c == '_') c = ' ';
  return String(r.c_str());
}

void setup() {
  Serial.begin(serialBaud);
  delay(10);
  Serial.println("Starting BLE peripheral...");
  // Initialize preferences and load warm duration
  prefs.begin("settings", false);
  warmDurationMinutes = prefs.getInt("warmlen", warmDurationMinutes);
  Serial.printf("Warm duration loaded: %d minutes\n", warmDurationMinutes);

  // Allow a short window to change baud by typing e.g. "b9600" or "b115200"
  Serial.println("Type 'b9600' or 'b115200' within 3s to change baud.");
  unsigned long t0 = millis();
  while (millis() - t0 < 3000) {
    if (Serial.available()) {
      String s = Serial.readStringUntil('\n');
      s.trim();
      if (s.length() > 1 && s.charAt(0) == 'b') {
        uint32_t nb = s.substring(1).toInt();
        if (nb >= 300 && nb <= 2000000) {
          serialBaud = nb;
          Serial.begin(serialBaud);
          Serial.print("Baud changed to "); Serial.println(serialBaud);
          break;
        }
      }
    }
  }

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(PIN_LOCK, OUTPUT);
  pinMode(PIN_UNLOCK, OUTPUT);
  pinMode(PIN_HAZZARD, OUTPUT);
  // engine/lighting pins
  pinMode(PIN_ACC, OUTPUT);
  pinMode(PIN_IG, OUTPUT);
  pinMode(PIN_STARTER, OUTPUT);
  pinMode(PIN_LAMP, OUTPUT);
  pinMode(PIN_ALARM, OUTPUT);
  digitalWrite(PIN_LOCK, LOW);
  digitalWrite(PIN_UNLOCK, LOW);
  digitalWrite(PIN_HAZZARD, LOW);
  digitalWrite(PIN_ACC, LOW);
  digitalWrite(PIN_IG, LOW);
  digitalWrite(PIN_STARTER, LOW);
  digitalWrite(PIN_LAMP, LOW);
  digitalWrite(PIN_ALARM, LOW);
  // Pesawat indicator default off
  pinMode(PIN_PESAWAT, OUTPUT);
  digitalWrite(PIN_PESAWAT, LOW);

  // Remote control inputs (433MHz RX580) - mapped to A/B/C/D
  pinMode(LEDIN_A, INPUT);
  pinMode(LEDIN_B, INPUT);
  pinMode(LEDIN_C, INPUT);
  pinMode(LEDIN_D, INPUT);

  Serial.print("Using serial baud: "); Serial.println(serialBaud);

  // Initialize RTC
  rtc.begin();

  // Initialize RX500 remote handler and register callbacks
  rx500.begin();
  rx500.setOnLock([](){ startLockPulse(); });
  rx500.setOnUnlock([](){ startUnlockPulse(); });
  rx500.setOnStart([&]() {
    if (!engineOn) {
      digitalWrite(PIN_ACC, HIGH); accOn = true;
      digitalWrite(PIN_IG, HIGH); igOn = true;
      startCarPending = true; startCarAt = millis() + 1000;
      setEngineState(true);
      ble.notify(std::string("ACK:START THE CAR SCHEDULED"));
      Serial.print("Remote: "); Serial.println(prettyCmd("start_the_car") + " scheduled (ACC+IG ON, starter in 1s)");
    } else {
      resetAll();
    }
  });
  rx500.setOnAlarmToggle([&]() {
    if (!alarmOn) {
      alarmOn = true;
      hazardAlarmMode = true;
      hazardActive = true;
      hazardStep = 0;
      hazardNext = millis();
      ble.notify(std::string("ACK:ALARM ON"));
      Serial.print("Remote: "); Serial.println(prettyCmd("alarm_on"));
    } else {
      alarmOn = false;
      hazardAlarmMode = false;
      hazardActive = false;
      digitalWrite(PIN_HAZZARD, LOW);
      ble.notify(std::string("ACK:ALARM OFF"));
      Serial.print("Remote: "); Serial.println(prettyCmd("alarm_off"));
    }
  });

  // Initialize physical button module (button pin 18, LED_POWER use PIN_PESAWAT if available)
  // We'll use PIN_PESAWAT as LED_POWER; change in ButtonTombol constructor if desired
  buttonTombol.begin();
  buttonTombol.setResetCallback([](){ resetAll(); });
  buttonTombol.setEngineSetter([&](bool v){ setEngineState(v); });
  // Initialize BLE module and register handlers
  // Register write and connection handlers; write handler performs Lock/Unlock logic
  ble.begin("ESP32-BLE-Mobile",
    // write handler
    [&](const std::string& val) {
      Serial.print("Characteristic written: "); Serial.println(val.c_str());
      std::string cmd = normalizeCmd(val);

      // ACC
      if (cmd == "acc_on") {
        digitalWrite(PIN_ACC, HIGH);
        accOn = true;
        ble.notify(std::string("ACK:ACC ON"));
        Serial.print("Action: "); Serial.println(prettyCmd("acc_on"));
        return;
      } else if (cmd == "acc_off") {
        digitalWrite(PIN_ACC, LOW);
        accOn = false;
        ble.notify(std::string("ACK:ACC OFF"));
        Serial.print("Action: "); Serial.println(prettyCmd("acc_off"));
        return;
      }

      // IG
      if (cmd == "ig_on") {
        digitalWrite(PIN_IG, HIGH);
        igOn = true;
        ble.notify(std::string("ACK:IG ON"));
        Serial.print("Action: "); Serial.println(prettyCmd("ig_on"));
        return;
      } else if (cmd == "ig_off") {
        digitalWrite(PIN_IG, LOW);
        igOn = false;
        ble.notify(std::string("ACK:IG OFF"));
        Serial.print("Action: "); Serial.println(prettyCmd("ig_off"));
        return;
      }

      // Composite command: Start_the_Car -> ACC_ON, IG_ON, then starter after 1s
      if (cmd == "start_the_car") {
        // Turn ACC and IG on immediately
        digitalWrite(PIN_ACC, HIGH);
        accOn = true;
        digitalWrite(PIN_IG, HIGH);
        igOn = true;
        // Schedule starter pulse after ~1000ms (handled non-blocking in loop())
        startCarPending = true;
        startCarAt = millis() + 1000;
        ble.notify(std::string("ACK:START THE CAR"));
        Serial.print("Action: "); Serial.println(prettyCmd("start_the_car"));
        return;
      }

      // STARTER (pulse 1000ms)
      if (cmd == "starter_on") {
        if (!starterActive) {
          digitalWrite(PIN_STARTER, HIGH);
          starterActive = true;
          starterEnd = millis() + 1000;
          setEngineState(true);
          ble.notify(std::string("ACK:STARTER ON"));
          Serial.print("Action: "); Serial.println(prettyCmd("starter_on"));
        } else {
          ble.notify(std::string("IGNORED:STARTER ON"));
          Serial.println("Ignored STARTER_ON (already active)");
        }
        return;
      }

      // ALARM
      if (cmd == "alarm_on") {
        alarmOn = true;

        hazardAlarmMode = true;
        hazardActive = true;
        hazardStep = 0;
        hazardNext = millis();

        ble.notify(std::string("ACK:ALARM ON"));
        Serial.print("Action: "); Serial.println(prettyCmd("alarm_on"));
        return;
      }
      else if (cmd == "alarm_off") {
        alarmOn = false;
        hazardAlarmMode = false;
        hazardActive = false;
        digitalWrite(PIN_HAZZARD, LOW);

        ble.notify(std::string("ACK:ALARM OFF"));
        Serial.print("Action: "); Serial.println(prettyCmd("alarm_off"));
        return;
      }


      // LAMP
      if (cmd == "lamp_on") {
        digitalWrite(PIN_LAMP, HIGH);
        lampOn = true;
        ble.notify(std::string("ACK:LAMP ON"));
        Serial.print("Action: "); Serial.println(prettyCmd("lamp_on"));
        return;
      } else if (cmd == "lamp_off") {
        digitalWrite(PIN_LAMP, LOW);
        lampOn = false;
        ble.notify(std::string("ACK:LAMP OFF"));
        Serial.print("Action: "); Serial.println(prettyCmd("lamp_off"));
        return;
      }

      // Lock/Unlock existing logic
      if (cmd == "lock") {
        startLockPulse();
      } else if (cmd == "unlock") {
        startUnlockPulse();
      } else {
        std::string resp = std::string("UNKNOWN:") + std::string(prettyCmd(val).c_str());
        ble.notify(resp);
        Serial.println("Action: UNKNOWN command");
      }
    },
    // connection handler
    [](bool connected) {
      isConnected = connected;
      digitalWrite(LED_PIN, connected ? HIGH : LOW);
      Serial.print("BLE Central "); Serial.println(connected ? "connected" : "disconnected");
      // if (!connected) {
      //   ble.startAdvertising(); // atau fungsi sejenis di BLEModule
      // }
    }
    
  );


  // Print the values you need for MIT App Inventor
  String mac = BLEDevice::getAddress().toString().c_str();
  Serial.println("--- BLE Info (use these in App Inventor) ---");
  Serial.print("Device Name: "); Serial.println("ESP32-BLE-Mobile");
  Serial.print("Device Address (MAC): "); Serial.println(mac);
  Serial.print("Service UUID: "); Serial.println(SERVICE_UUID.toString().c_str());
  Serial.print("Characteristic UUID: "); Serial.println(CHARACTERISTIC_UUID.toString().c_str());
  Serial.println("-------------------------------------------");
}

void loop() {
  // Report connection state only when it changes to avoid flooding the log
  static unsigned long loopTick = 0;
  if (millis() - loopTick < 10) return;
  loopTick = millis();

  static bool lastState = false;
  if (isConnected != lastState) {
    lastState = isConnected;
    Serial.print("isConnected: "); Serial.println(isConnected ? "true" : "false");
  }
  // Complete any active pulse and set locked/unlocked state
  if (pulseActive && millis() >= pulseEnd) {
    // turn off the pulse pin
    digitalWrite(pulsePin, LOW);
    pulseActive = false;
    if (pulsePin == PIN_LOCK) {
      locked = true;
      Serial.println("Locked: true");
      ble.notify(std::string("LOCKED"));
       // START hazard LOCK (2x kedip)
      hazardActive = false;
      hazardLockedMode = true;
      hazardStep = 0;
      hazardNext = millis();
      hazardActive = true;

      // Pesawat indicator turns on and starts blinking while locked
      pesawatOn = true;
      pesawatStateHigh = true;
      digitalWrite(PIN_PESAWAT, HIGH);
      pesawatNextToggle = millis() + 300; // high duration 300ms
    } else if (pulsePin == PIN_UNLOCK) {
      locked = false;
      Serial.println("Locked: false (unlocked)");
      ble.notify(std::string("UNLOCKED"));
      
  // START hazard UNLOCK (1x panjang)
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

  // Complete starter pulse
  if (starterActive && millis() >= starterEnd) {
    digitalWrite(PIN_STARTER, LOW);
    starterActive = false;
    // Keep `engineOn` true after starter pulse so subsequent C press can trigger resetAll()
    Serial.println("Starter pulse complete (engineOn state unchanged)");
    ble.notify(std::string("ACK:STARTER_DONE"));
  }

  // Alarm blinking (non-blocking)
  if (alarmOn && millis() >= alarmNextToggle) {
    if (alarmStateHigh) {
      digitalWrite(PIN_ALARM, LOW);
      alarmStateHigh = false;
      alarmNextToggle = millis() + 200; // off duration
    } else {
      digitalWrite(PIN_ALARM, HIGH);
      alarmStateHigh = true;
      alarmNextToggle = millis() + 200; // on duration
    }
  }

  // Pesawat blinking (while pesawatOn)
  if (pesawatOn && millis() >= pesawatNextToggle) {
    if (pesawatStateHigh) {
      digitalWrite(PIN_PESAWAT, LOW);
      pesawatStateHigh = false;
      pesawatNextToggle = millis() + 3000; // low duration 3000ms
    } else {
      digitalWrite(PIN_PESAWAT, HIGH);
      pesawatStateHigh = true;
      pesawatNextToggle = millis() + 100; // high duration 300ms
    }
  }

  // Hazard light control (non-blocking state machine)
  // ===== Hazard non-blocking =====
  if (hazardActive && millis() >= hazardNext) {
    // Priority: continuous alarm blinking mode
    if (hazardAlarmMode) {
      // Toggle hazard continuously (200ms on/off)
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
      // MODE LOCK: 2x kedip
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
      // MODE UNLOCK: 1x panjang
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



  // Remote inputs handled by RX500Module
  rx500.update();
  // Update physical button module
  buttonTombol.update();

// Periodically print RTC time for logging (every 10s)
  static unsigned long lastRtc = 0;
  if (millis() - lastRtc > 10000) {
    lastRtc = millis();
    rtc.printNow();
  }

  // Serial command handling for on-device testing
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      Serial.print("Serial cmd: "); Serial.println(cmd);
      if (cmd.equalsIgnoreCase("rtc")) {
        Serial.print("RTC: "); Serial.println(rtc.nowString());
      } else if (cmd.equalsIgnoreCase("i2cscan")) {
        Serial.println("I2C scan start");
        Wire.begin();
        for (uint8_t addr = 1; addr < 127; ++addr) {
          Wire.beginTransmission(addr);
          if (Wire.endTransmission() == 0) {
            Serial.print("Found I2C device at 0x"); Serial.println(addr, HEX);
          }
        }
        Serial.println("I2C scan done");
      } else if (cmd.equalsIgnoreCase("warm")) {
        if (!warmActive) {
          warmActive = true;
          warmEnd = millis() + ((unsigned long)warmDurationMinutes * 60UL * 1000UL);
          digitalWrite(PIN_IG, HIGH); igOn = true;
          warmStarterPending = true; warmStarterAt = millis() + 1000;
          Serial.printf("Warm-up forced: IG_ON, starter in 1s, duration %d min\n", warmDurationMinutes);
          ble.notify(std::string("ACK:WARM_ON"));
        } else {
          Serial.println("Warm-up already active");
        }
      } else if (cmd.equalsIgnoreCase("lock")) {
        startLockPulse();
      } else if (cmd.equalsIgnoreCase("unlock")) {
        startUnlockPulse();
      } else if (cmd.startsWith("setrtc")) {
        // setrtc [now|YYYY-MM-DD HH:MM:SS]
        String arg = cmd.substring(6);
        arg.trim();
        if (arg.length() == 0 || arg.equalsIgnoreCase("now")) {
          rtc.setNowToCompileTime();
          Serial.println("RTC set to compile time");
          rtc.printNow();
        } else {
          if (rtc.setNowFromString(arg)) {
            Serial.println("RTC set to:"); rtc.printNow();
          } else {
            Serial.println("Invalid datetime format. Use: setrtc YYYY-MM-DD HH:MM:SS or setrtc now");
          }
        }
      } else if (cmd.startsWith("warmlen")) {
        String arg = cmd.substring(7);
        arg.trim();
        if (arg.length() > 0) {
          int m = arg.toInt();
          if (m >= 1 && m <= 60) {
            warmDurationMinutes = m;
            warmDurationMinutes = m;
            prefs.putInt("warmlen", warmDurationMinutes);
            Serial.printf("Warm duration set to %d minutes (saved)\n", warmDurationMinutes);
          } else {
            Serial.println("Invalid minutes (1-60)");
          }
        } else {
          Serial.printf("Current warm duration: %d minutes\n", warmDurationMinutes);
        }
      } else if (cmd.equalsIgnoreCase("help")) {
        Serial.println("Commands: rtc, i2cscan, warm, warmlen [min], setrtc [now|YYYY-MM-DD HH:MM:SS], lock, unlock, help");
      } else {
        Serial.println("Unknown serial command. Type 'help' for list");
      }
    }
  }

  // Daily warm-up trigger at 08:00 (once per day)
  String now = rtc.nowString();
  int hour = now.substring(11,13).toInt();
  int minute = now.substring(14,16).toInt();
  int day = now.substring(8,10).toInt();
  if (hour == 8 && minute == 0 && lastWarmDay != day) {
    // Start warm routine: igOn, then after 1000ms starter pulse, run for configured minutes
    lastWarmDay = day;
    warmActive = true;
    warmEnd = millis() + ((unsigned long)warmDurationMinutes * 60UL * 1000UL);
    // IG on immediately
    digitalWrite(PIN_IG, HIGH);
    igOn = true;
    // schedule starter after 1000ms
    warmStarterPending = true;
    warmStarterAt = millis() + 1000;
    ble.notify(std::string("ACK:WARM_ON"));
    Serial.printf("Warm-up scheduled: IG_ON, starter in 1s, duration %d min\n", warmDurationMinutes);
  }

  // Execute warm starter pending
  if (warmStarterPending && millis() >= warmStarterAt) {
    warmStarterPending = false;
    if (!starterActive) {
      digitalWrite(PIN_STARTER, HIGH);
      starterActive = true;
      starterEnd = millis() + 1000; // starter pulse 1s
      setEngineState(true);
      Serial.println("Warm-up: STARTER pulse started (1s)");
    }
  }

  // Execute Start_the_Car pending (scheduled by BLE command Start_the_Car)
  if (startCarPending && millis() >= startCarAt) {
    startCarPending = false;
    if (!starterActive) {
      digitalWrite(PIN_STARTER, HIGH);
      starterActive = true;
      starterEnd = millis() + 1000; // starter pulse 1s
      setEngineState(true);
      ble.notify(std::string("ACK:STARTER ON"));
      Serial.println("Start_the_Car: STARTER pulse started (1s)");
    }
  }

  // Finish warm period
  if (warmActive && millis() >= warmEnd) {
    warmActive = false;
    // semuaOff: turn off ACC, IG, STARTER, LAMP, ALARM (keep pesawat controlled by lock state)
    digitalWrite(PIN_ACC, LOW); accOn = false;
    digitalWrite(PIN_IG, LOW); igOn = false;
    digitalWrite(PIN_STARTER, LOW); starterActive = false; engineOn = false;
    digitalWrite(PIN_LAMP, LOW); lampOn = false;
    digitalWrite(PIN_ALARM, LOW); alarmOn = false; alarmStateHigh = false;
    ble.notify(std::string("ACK:WARM_DONE"));
    Serial.println("Warm-up complete: systems turned off (pesawat unaffected)");
  }

  // delay(200); // no delay to keep responsiveness



}

/*
Optional: Central (scanner) flow to discover devices and read services/characteristics:
- Use BLEDevice::getScan() and a custom callback to list found devices and addresses.
- To enumerate characteristics you must connect to the device and call getServices()/getCharacteristics().
This example focuses on the peripheral side because MIT App Inventor typically connects to a peripheral device.
*/