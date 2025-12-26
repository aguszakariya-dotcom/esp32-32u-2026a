// #include "RTCModule.h"

// #include <Wire.h>

// RTCModule::RTCModule() {}

// bool RTCModule::begin() {
//   // Use explicit I2C pins for ESP32 so wiring is deterministic
//   Wire.begin(SDA_PIN, SCL_PIN);
//   Serial.print("RTC: initializing on SDA="); Serial.print(SDA_PIN);
//   Serial.print(" SCL="); Serial.println(SCL_PIN);

//   if (!rtc.begin()) {
//     Serial.println("RTC not found");
//     return false;
//   }
//   if (rtc.lostPower()) {
//     Serial.println("RTC lost power, setting to compile time");
//     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
//   }
//   return true;
// }

// String RTCModule::nowString() {
//   DateTime t = rtc.now();
//   char buf[32];
//   snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u", t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
//   return String(buf);
// }

// void RTCModule::printNow() {
//   Serial.print("RTC: ");
//   Serial.println(nowString());
// }

// bool RTCModule::setNowFromString(const String& s) {
//   // Expect format YYYY-MM-DD HH:MM:SS
//   if (s.length() < 19) return false;
//   int y = s.substring(0,4).toInt();
//   int m = s.substring(5,7).toInt();
//   int d = s.substring(8,10).toInt();
//   int hh = s.substring(11,13).toInt();
//   int mm = s.substring(14,16).toInt();
//   int ss = s.substring(17,19).toInt();
//   if (y < 2000 || m < 1 || m > 12 || d < 1 || d > 31) return false;
//   rtc.adjust(DateTime(y, m, d, hh, mm, ss));
//   return true;
// }

// void RTCModule::setNowToCompileTime() {
//   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
// }
#include "RTCModule.h"
#include <Wire.h>

RTCModule::RTCModule() {
  rtcStatus = RTC_NEED_SET;
}

bool RTCModule::begin() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.printf("RTC init SDA=%d SCL=%d\n", SDA_PIN, SCL_PIN);

  if (!rtc.begin()) {
    Serial.println("RTC NOT FOUND");
    rtcStatus = RTC_NEED_SET;
    return false;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power -> NEED SET");
    rtcStatus = RTC_NEED_SET;
  } else {
    rtcStatus = RTC_OK;
  }

  return true;
}

RTCStatus RTCModule::status() {
  return rtcStatus;
}

DateTime RTCModule::now() {
  // Avoid calling rtc.now() (I2C) when RTC status is not OK to prevent
  // repeated Wire errors when the device is missing or the bus is faulty.
  if (rtcStatus != RTC_OK) {
    return DateTime(2000, 1, 1, 0, 0, 0);
  }
  return rtc.now();
}

String RTCModule::nowString() {
  // If RTC not set or lost power, report safe neutral time 2000-01-01 00:00:00
  if (rtcStatus != RTC_OK || rtc.lostPower()) {
    return String("2000-01-01 00:00:00");
  }
  DateTime t = rtc.now();
  char buf[32];
  snprintf(buf, sizeof(buf),
           "%04d-%02d-%02d %02d:%02d:%02d",
           t.year(), t.month(), t.day(),
           t.hour(), t.minute(), t.second());
  return String(buf);
}

void RTCModule::printNow() {
  Serial.print("RTC: ");
  Serial.println(nowString());
}

bool RTCModule::setNowFromString(const String& s) {
  if (s.length() < 19) return false;

  int y  = s.substring(0,4).toInt();
  int m  = s.substring(5,7).toInt();
  int d  = s.substring(8,10).toInt();
  int hh = s.substring(11,13).toInt();
  int mm = s.substring(14,16).toInt();
  int ss = s.substring(17,19).toInt();

  if (y < 2020 || m < 1 || m > 12 || d < 1 || d > 31) {
    return false;
  }

  rtc.adjust(DateTime(y, m, d, hh, mm, ss));
  rtcStatus = RTC_OK;

  Serial.println("RTC manually set -> OK");
  return true;
}

bool RTCModule::lostPowerFlag() {
  // If RTC isn't initialized or flagged as NEED_SET, avoid querying over I2C.
  if (rtcStatus != RTC_OK) return true;
  return rtc.lostPower();
}
