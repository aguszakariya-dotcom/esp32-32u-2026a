#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <functional>
#include "RTCModule.h"

class WarmUpEngine {
public:
  WarmUpEngine();
  void init(RTCModule* rtc, Preferences* prefs, std::function<void(bool)> engineSetter);
  void begin();
  void update();
  void forceWarm();
  void cancelWarm();
  void setDurationMinutes(int m);
  int getDurationMinutes() const;
  bool isActive() const;
  unsigned long remainingMillis() const;
private:
  RTCModule* _rtc;
  Preferences* _prefs;
  std::function<void(bool)> _engineSetter;
  bool warmActive;
  unsigned long warmEnd;
  bool warmStarterPending;
  unsigned long warmStarterAt;
  int lastWarmDay;
  int warmDurationMinutes;
  bool rtcTimePlausible() const;
  DateTime rtcSafeNow() const;
};
