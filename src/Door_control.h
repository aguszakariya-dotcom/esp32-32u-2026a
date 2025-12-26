#pragma once
#include <Arduino.h>
#include <functional>

class DoorControl {
public:
  DoorControl();
  void begin();
  void update();
  void lockPulse();
  void unlockPulse();
  void toggleAlarm();
  void setAlarm(bool on);
  void setEngineState(bool on);
  void cancelAll();
  bool isLocked() const;
private:
  bool locked;
  bool pulseActive;
  int pulsePin;
  unsigned long pulseEnd;
  bool pesawatOn;
  bool pesawatStateHigh;
  unsigned long pesawatNextToggle;
  bool hazardActive;
  bool hazardLockedMode;
  int hazardStep;
  unsigned long hazardNext;
  bool hazardAlarmMode;
  bool alarmOn;
  bool alarmStateHigh;
  unsigned long alarmNextToggle;
};
