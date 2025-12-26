#include "RTCModule.h"

#include <Wire.h>

RTCModule::RTCModule() {}

bool RTCModule::begin() {
  // Use explicit I2C pins for ESP32 so wiring is deterministic
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.print("RTC: initializing on SDA="); Serial.print(SDA_PIN);
  Serial.print(" SCL="); Serial.println(SCL_PIN);

  if (!rtc.begin()) {
    Serial.println("RTC not found");
    return false;
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting to compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  return true;
}

String RTCModule::nowString() {
  DateTime t = rtc.now();
  char buf[32];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u", t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
  return String(buf);
}

void RTCModule::printNow() {
  Serial.print("RTC: ");
  Serial.println(nowString());
}

bool RTCModule::setNowFromString(const String& s) {
  // Expect format YYYY-MM-DD HH:MM:SS
  if (s.length() < 19) return false;
  int y = s.substring(0,4).toInt();
  int m = s.substring(5,7).toInt();
  int d = s.substring(8,10).toInt();
  int hh = s.substring(11,13).toInt();
  int mm = s.substring(14,16).toInt();
  int ss = s.substring(17,19).toInt();
  if (y < 2000 || m < 1 || m > 12 || d < 1 || d > 31) return false;
  rtc.adjust(DateTime(y, m, d, hh, mm, ss));
  return true;
}

void RTCModule::setNowToCompileTime() {
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}
