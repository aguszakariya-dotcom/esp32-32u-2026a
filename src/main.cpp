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
#include "WarmUp_engine.h"
#include "Door_control.h"

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
// Whether we've received HOSTTIME from the host
static bool hostTimeSynced = false;
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
bool starterActive = false;
unsigned long starterEnd = 0;
// Start_the_Car composite command flags
static volatile bool startCarPending = false;
static volatile unsigned long startCarAt = 0;
// start sequence stage: 0 = waiting for IG, 1 = waiting for STARTER
static volatile int startCarStage = 0;

// resetAll delayed turn-off
static bool resetPending = false;
static unsigned long resetAt = 0;

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

// Preferences (NVS) to persist settings
static Preferences prefs;

BLEModule ble;
RTCModule rtc;
RX500Module rx500;
ButtonTombol buttonTombol(18, PIN_LED_POWER); // button pin 18, LED_POWER on PIN_LED_POWER (GPIO13)
WarmUpEngine warmEngine;
DoorControl doorControl;

// Normalize incoming command string to lowercase (preserve underscores)
static std::string normalizeCmd(const std::string &s) {
  std::string r = s;
  // trim leading/trailing spaces
  while (!r.empty() && std::isspace((unsigned char)r.front())) r.erase(r.begin());
  while (!r.empty() && std::isspace((unsigned char)r.back())) r.pop_back();
  std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
  return r;
}

// BLE notifications are limited by ATT MTU (commonly 20 bytes payload).
// Split longer messages into 20-byte chunks so the app receives the full text.
static void bleNotifyChunks(const std::string &s) {
  const size_t CHUNK = 20;
  for (size_t i = 0; i < s.size(); i += CHUNK) {
    std::string part = s.substr(i, CHUNK);
    ble.notify(part);
    delay(5);
  }
}



// Pretty-print command for serial output: uppercase and replace '_' with ' '
static String prettyCmd(const std::string &s) {
  std::string r = s;
  std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::toupper(c); });
  for (auto &c : r) if (c == '_') c = ' ';
  return String(r.c_str());
}

// Set engine state and notify DoorControl and button module
void setEngineState(bool on) {
  engineOn = on;
  doorControl.setEngineState(on);
  buttonTombol.setEngineStatus(on);
}

// Reset all outputs/state (called from remote or other flows)
void resetAll() {
  // Immediately turn off IG, starter and alarm; schedule ACC off after 500ms
  digitalWrite(PIN_IG, LOW); igOn = false;
  digitalWrite(PIN_STARTER, LOW); starterActive = false;
  digitalWrite(PIN_ALARM, LOW); alarmOn = false; alarmStateHigh = false;
  setEngineState(false);
  // Cancel any pending start/warm operations
  startCarPending = false; startCarStage = 0;
  warmEngine.cancelWarm();
  // Cancel door pulses/hazard
  doorControl.cancelAll();
  // Schedule ACC and other outputs off after 500ms
  resetPending = true;
  resetAt = millis() + 500;
  ble.notify(std::string("RESET ALL SCHEDULED"));
  Serial.println("Action: RESET_ALL scheduled (IG OFF now, ACC OFF in 500ms)");
}

void setup() {
  Serial.begin(serialBaud);
  delay(10);
  Serial.println("Starting BLE peripheral...");
  // Initialize preferences and load warm duration
  prefs.begin("settings", false);
  // initialize WarmUp engine and Door control
  warmEngine.init(&rtc, &prefs, [](bool v){ setEngineState(v); });
  warmEngine.begin();
  doorControl.begin();
  // Load saved button countdown (ms) if present
  int savedBtnCd = prefs.getInt("btncd", 15000);
  buttonTombol.setCountdownMs((unsigned long)savedBtnCd);
  Serial.printf("Button countdown loaded: %d ms\n", savedBtnCd);

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
  rx500.setOnLock([&](){ doorControl.lockPulse(); });
  rx500.setOnUnlock([&](){ doorControl.unlockPulse(); });
  rx500.setOnStart([&]() {
    if (!engineOn) {
      // use same flow as button to start with countdown
      buttonTombol.triggerStart();
      ble.notify(std::string("START THE CAR SCHEDULED"));
      Serial.print("Remote: "); Serial.println(prettyCmd("start_the_car") + " triggered (countdown active)");
    } else {
      resetAll();
    }
  });
  rx500.setOnAlarmToggle([&]() { doorControl.toggleAlarm(); });

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
        ble.notify(std::string("ACC ON"));
        Serial.print("Action: "); Serial.println(prettyCmd("acc_on"));
        return;
      } else if (cmd == "acc_off") {
        digitalWrite(PIN_ACC, LOW);
        accOn = false;
        ble.notify(std::string("ACC OFF"));
        Serial.print("Action: "); Serial.println(prettyCmd("acc_off"));
        return;
      }

      // IG
      if (cmd == "ig_on") {
        digitalWrite(PIN_IG, HIGH);
        igOn = true;
        ble.notify(std::string("IGNITION ON"));
        Serial.print("Action: "); Serial.println(prettyCmd("ig_on"));
        return;
      } else if (cmd == "ig_off") {
        digitalWrite(PIN_IG, LOW);
        igOn = false;
        ble.notify(std::string("IGNITION OFF"));
        Serial.print("Action: "); Serial.println(prettyCmd("ig_off"));
        return;
      }

      // Composite command: Start_the_Car -> ACC_ON, IG_ON, then starter after 1s
      if (cmd == "start_the_car") {
        // Prevent duplicate starts: ignore if engine already on, starter active, or pending
        // Prevent duplicate starts: ignore if engine already on, starter active, or pending
        if (engineOn || starterActive || startCarPending) {
          ble.notify(std::string("ENGINE ALREADY ON"));
          Serial.println("Ignored START_THE_CAR (engine on or start pending)");
          return;
        }
        // Trigger the same start flow as the physical button (starts countdown)
        buttonTombol.triggerStart();
        ble.notify(std::string("START THE CAR"));
        Serial.print("Action: "); Serial.println(prettyCmd("start_the_car") + " triggered (countdown active)");
        return;
      }

      // STARTER (pulse 1000ms)
      if (cmd == "starter_on") {
        if (!starterActive) {
          digitalWrite(PIN_STARTER, HIGH);
          starterActive = true;
          starterEnd = millis() + 1000;
          setEngineState(true);
          ble.notify(std::string("STARTER ON"));
          Serial.print("Action: "); Serial.println(prettyCmd("starter_on"));
        } else {
          ble.notify(std::string("IGNORED STARTER ON"));
          Serial.println("Ignored STARTER_ON (already active)");
        }
        return;
      }

      // ALARM
      if (cmd == "alarm_on") {
        doorControl.setAlarm(true);
        return;
      }
      else if (cmd == "alarm_off") {
        doorControl.setAlarm(false);
        return;
      }


      // LAMP
      if (cmd == "lamp_on") {
        digitalWrite(PIN_LAMP, HIGH);
        lampOn = true;
        ble.notify(std::string("LAMP ON"));
        Serial.print("Action: "); Serial.println(prettyCmd("lamp_on"));
        return;
      } else if (cmd == "lamp_off") {
        digitalWrite(PIN_LAMP, LOW);
        lampOn = false;
        ble.notify(std::string("LAMP OFF"));
        Serial.print("Action: "); Serial.println(prettyCmd("lamp_off"));
        return;
      }

      // Lock/Unlock existing logic
      // RESET_ALL (from BLE string)
      if (cmd == "reset_all") {
        resetAll();
        return;
      }
      // BTN countdown via BLE: "btncd <ms>"
      if (cmd.rfind("btncd", 0) == 0) {
        std::string arg = cmd.substr(5);
        // trim leading spaces
        while (!arg.empty() && std::isspace((unsigned char)arg.front())) arg.erase(arg.begin());
        if (!arg.empty()) {
          try {
            unsigned long v = std::stoul(arg);
            if (v >= 5000 && v <= 60000) {
              buttonTombol.setCountdownMs(v);
              prefs.putInt("btncd", (int)v);
              ble.notify(std::string("BUTTON COUNTDOWN SET"));
              Serial.printf("Button countdown set to %lu ms\n", v);
            } else {
              ble.notify(std::string("BTNCD INVALID RANGE"));
              Serial.println("BTNCD invalid range (use 5000-60000 ms)");
            }
          } catch(...) {
            ble.notify(std::string("BTNCD PARSE ERROR"));
            Serial.println("BTNCD parse error");
          }
        } else {
          ble.notify(std::string("BTNCD USAGE"));
          Serial.println("Usage via BLE: btncd <ms>");
        }
        return;
      }
      // SETRTC via BLE: "setrtc [now|YYYY-MM-DD HH:MM:SS]"
      if (cmd.rfind("setrtc", 0) == 0) {
        std::string arg = cmd.substr(6);
        while (!arg.empty() && std::isspace((unsigned char)arg.front())) arg.erase(arg.begin());
        if (arg.empty() || arg == "now") {
          ble.notify(std::string("RTC SET DECLINED"));
          Serial.println("Refusing to set RTC to compile-time or 'now' via BLE. Use: setrtc YYYY-MM-DD HH:MM:SS or send HOSTTIME from host.");
        } else {
          String s(arg.c_str());
          if (rtc.setNowFromString(s)) {
            ble.notify(std::string("RTC SET"));
            Serial.println("RTC set via BLE:");
            {
              String now = rtc.nowString();
              Serial.print("RTC: "); Serial.println(now);
              bleNotifyChunks(std::string(now.c_str()));
            }
          } else {
            ble.notify(std::string("RTC SET FAILED"));
            Serial.println("Invalid datetime format via BLE. Use: setrtc YYYY-MM-DD HH:MM:SS");
          }
        }
        return;
      }
      if (cmd == "lock") {
        doorControl.lockPulse();
      } else if (cmd == "unlock") {
        doorControl.unlockPulse();
      } else {
        std::string resp = std::string("UNKNOWN ") + std::string(prettyCmd(val).c_str());
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
  // Do not wait for HOSTTIME on startup; use RTC as-is for serial and scheduling.
  Serial.println("Not waiting for HOSTTIME; using RTC value for scheduling if plausible.");
  hostTimeSynced = true; // prevent periodic GETTIME requests
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
  

  // Update door control (handles lock/unlock pulses, hazard, pesawat, alarm blinking)
  doorControl.update();
  // Update warm engine scheduler and warm starter handling
  warmEngine.update();



  // Remote inputs handled by RX500Module
  rx500.update();
  // Update physical button module
  buttonTombol.update();

// Periodically print RTC time for logging (every 10s)
  static unsigned long lastRtc = 0;
  if (millis() - lastRtc > 10000) {
    lastRtc = millis();
    if (warmEngine.isActive()) {
      unsigned long rem = warmEngine.remainingMillis();
      unsigned int rhour = (unsigned int)(rem / 3600000UL);
      unsigned int rmin = (unsigned int)((rem / 60000UL) % 60UL);
      unsigned int rsec = (unsigned int)((rem / 1000UL) % 60UL);
      char buf[32];
      snprintf(buf, sizeof(buf), "%02u:%02u:%02u", rhour, rmin, rsec);
      Serial.print("WARM: "); Serial.println(buf);
      std::string notif = std::string("WARM: ") + std::string(buf);
      bleNotifyChunks(notif);
    } else {
      String now = rtc.nowString();
      Serial.print("RTC: "); Serial.println(now);
      bleNotifyChunks(std::string(now.c_str()));
    }
  }

  // If host time not yet synced, periodically request it (every 30s)
  static unsigned long lastHostRequest = 0;
  const unsigned long HOST_REQUEST_INTERVAL = 30000;
  if (!hostTimeSynced && millis() - lastHostRequest > HOST_REQUEST_INTERVAL) {
    Serial.println("GETTIME");
    lastHostRequest = millis();
  }

  // Serial command handling for on-device testing
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      Serial.print("Serial cmd: "); Serial.println(cmd);
      if (cmd.startsWith("HOSTTIME")) {
        String arg = cmd.substring(8);
        arg.trim();
        if (arg.length() > 0 && rtc.setNowFromString(arg)) {
          Serial.println("RTC set from host (serial)");
          {
            String now = rtc.nowString();
            Serial.print("RTC: "); Serial.println(now);
            bleNotifyChunks(std::string(now.c_str()));
          }
        } else {
          Serial.println("Invalid HOSTTIME format");
        }
      }
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
        warmEngine.forceWarm();
      } else if (cmd.equalsIgnoreCase("lock")) {
        doorControl.lockPulse();
      } else if (cmd.equalsIgnoreCase("unlock")) {
        doorControl.unlockPulse();
      } else if (cmd.startsWith("setrtc")) {
        // setrtc [now|YYYY-MM-DD HH:MM:SS]
        String arg = cmd.substring(6);
        arg.trim();
        if (arg.length() == 0 || arg.equalsIgnoreCase("now")) {
          Serial.println("Refusing to set RTC to compile-time or 'now'. Use: setrtc YYYY-MM-DD HH:MM:SS or send HOSTTIME.");
        } else {
          if (rtc.setNowFromString(arg)) {
            Serial.println("RTC set to:");
            {
              String now = rtc.nowString();
              Serial.print("RTC: "); Serial.println(now);
              ble.notify(std::string("RTC: ") + std::string(now.c_str()));
            }
          } else {
            Serial.println("Invalid datetime format. Use: setrtc YYYY-MM-DD HH:MM:SS or setrtc now");
          }
        }
      } else if (cmd.startsWith("warmlen")) {
        String arg = cmd.substring(7);
        arg.trim();
        if (arg.length() > 0) {
          int m = arg.toInt();
          warmEngine.setDurationMinutes(m);
        } else {
          Serial.printf("Current warm duration: %d minutes\n", warmEngine.getDurationMinutes());
        }
      } else if (cmd.equalsIgnoreCase("help")) {
        Serial.println("Commands: rtc, i2cscan, warm, warmlen [min], setrtc [now|YYYY-MM-DD HH:MM:SS], lock, unlock, help");
      } else if (cmd.startsWith("btncd")) {
        // btncd [ms] -> set ButtonTombol countdown window in milliseconds
        String arg = cmd.substring(5);
        arg.trim();
        if (arg.length() > 0) {
          unsigned long v = (unsigned long)arg.toInt();
          if (v >= 5000 && v <= 60000) {
            buttonTombol.setCountdownMs(v);
            prefs.putInt("btncd", (int)v);
            Serial.printf("Button countdown set to %lu ms\n", v);
          } else {
            Serial.println("Invalid value. Use 5000-60000 ms (5-60s)");
          }
        } else {
          Serial.println("Usage: btncd [ms]  (e.g. btncd 20000)");
        }
      } else {
        Serial.println("Unknown serial command. Type 'help' for list");
      }
    }
  }

  // Warm-up scheduling and starter are managed by WarmUpEngine

  // Execute Start_the_Car pending (scheduled by BLE command Start_the_Car)
  if (startCarPending && millis() >= startCarAt) {
    // Stage 0 -> turn IG on and schedule starter
    if (startCarStage == 0) {
      digitalWrite(PIN_IG, HIGH);
      igOn = true;
      startCarStage = 1;
      startCarAt = millis() + 1000; // schedule starter in 1s
      ble.notify(std::string("IGNITION ON (START SEQUENCE)"));
      Serial.println("Start_the_Car: IG ON, starter scheduled in 1s");
    }
    // Stage 1 -> engage starter pulse
    else if (startCarStage == 1) {
      // finish sequence
      startCarPending = false;
      startCarStage = 0;
      if (!starterActive) {
        digitalWrite(PIN_STARTER, HIGH);
        starterActive = true;
        starterEnd = millis() + 1000; // starter pulse 1s
        setEngineState(true);
        ble.notify(std::string("STARTER ON"));
        Serial.println("Start_the_Car: STARTER pulse started (1s)");
      }
    }
  }

  // Execute reset pending: ACC off after IG off (500ms)
  if (resetPending && millis() >= resetAt) {
    resetPending = false;
    digitalWrite(PIN_ACC, LOW); accOn = false;
    digitalWrite(PIN_LAMP, LOW); lampOn = false;
    // ensure hazard/alarm off as part of full reset
    digitalWrite(PIN_HAZZARD, LOW);
    ble.notify(std::string("ALL OFF"));
    Serial.println("Reset sequence: ACC and other outputs turned off");
  }

  // Warm-up finish is handled by WarmUpEngine

  // delay(200); // no delay to keep responsiveness



}



/*
Optional: Central (scanner) flow to discover devices and read services/characteristics:
- Use BLEDevice::getScan() and a custom callback to list found devices and addresses.
- To enumerate characteristics you must connect to the device and call getServices()/getCharacteristics().
This example focuses on the peripheral side because MIT App Inventor typically connects to a peripheral device.
*/