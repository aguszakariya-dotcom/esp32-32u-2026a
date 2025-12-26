#pragma once

#include <Arduino.h>
#include <RTClib.h>

class RTCModule {
public:
  RTCModule();
  bool begin();
  String nowString();
  void printNow();
  // Set RTC time from a string "YYYY-MM-DD HH:MM:SS". Returns true on success.
  bool setNowFromString(const String& s);
  // Set RTC to compile time
  void setNowToCompileTime();
private:
  RTC_DS3231 rtc;
  // Default I2C pins for ESP32 (SDA, SCL)
  static const int SDA_PIN = 21;
  static const int SCL_PIN = 22;
};
