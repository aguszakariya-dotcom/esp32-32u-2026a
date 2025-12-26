// pin_config.h
// Centralized pin definitions taken from README_BLE.md
#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// Standard/engine LEDs and controls
#define PIN_ACC       25
#define PIN_IG        26
#define PIN_STARTER   27

// Central lock, connect LED, hazard, pesawat
#define LED_PIN       2
#define PIN_LOCK      4
#define PIN_UNLOCK    16
#define PIN_HAZZARD   17
#define PIN_PESAWAT   23

// Lamp / Alarm pins
#define PIN_LAMP      32
// Alarm output pin (used for ALARM_ON / ALARM_OFF). Default set to 33, change if needed.
#define PIN_ALARM     33

// Indicator LED for physical button/power (avoid using SCL/GPIO21/22)
#define PIN_LED_POWER 13

// Remote control inputs (433MHz RX580)
#define LEDIN_A       34
#define LEDIN_B       35
#define LEDIN_C       36
#define LEDIN_D       39

#endif // PIN_CONFIG_H
