ESP32 BLE helper
=================

saya ingin menggunakan/ menyambungkan dengan aplikasi Mit App Inventor,
--- BLE Info (use these in App Inventor) ---
Device Name: ESP32-BLE-Mobile
Device Address (MAC): 68:25:dd:e8:1f:6e
Service UUID: 12345678-1234-1234-1234-123456789abc
Characteristic UUID: abcdefab-1234-5678-1234-abcdefabcdef

Serial: default 115200 (you can change at runtime by typing `b9600` or `b115200` within 3s after reset)

saya ingin tidak butuh scanning, langsung menggunakan jika ditemukan misalnya "deviceName" = "ESP32-BLE-Mobile", atau deviceAddress = "68:25:dd:e8:1f:6e"
- jika ditemukan lakukan connecting.., jika tersambung, LabelInfo = "Terhubung", else "Tidak terhubung"

Perilaku tambahan:
- Saat koneksi BLE terdeteksi, board akan menyalakan LED internal pada pin GPIO2.
- Jika koneksi terputus, LED akan dimatikan dan advertising dilanjutkan.

-/------ Lock-Unlock -------\-
- Jika Ble mengirim text/ string "Lock", 'Locked = true
- Jika Ble mengirim text/ string "Unlock", maka 'Unlocked' = true
- jika 'Locked' = true, maka 'PIN_HAZZARD' berkedip 2x (On = 400--> Off, delay = 200, On 400--> Off).
- Jika 'Unlocked' = true, maka 'PIN_HAZARD' berkedip 1x (On 1000--> Off).


-/---------- ACC, IGNITION, STARTER, LAMPU, POSISI ------------\-
** PIN_ACC = 25 **
- jika Ble mengirim 'string' = 'ACC_ON' maka 'PIN_ACC', HIGH  .// 'accOn' = true
- jika Ble mengirim 'string' = 'ACC_OFF' maka 'PIN_ACC', LOW

** PIN_IG = 26 **
- jika Ble mengirim 'string' = 'IG_ON' maka 'PIN_IG', HIGH   .// 'igOn' = true
- jika Ble mengirim 'string' = 'IG_OFF' maka 'PIN_IG', LOW

** PIN_STARTER = 27 **
- jika Ble mengirim 'string' = 'STARTER_ON' maka {'PIN_STARTER', HIGH, delay 1000ms 'PIN_STARTER' LOW}    .// 'engineOn' = true

- jika Ble mengirim 'string' = 'ALARM_ON' maka 'PIN_ALARM', HIGH (BERKEDIP 200,200ms contunue) .//  'alarmOn' = true
- jika Ble mengirim 'string' = 'ALARM_OFF' maka 'PIN_ALARM', LOW (STOP BERKEDIP)

** PIN_LAMP = 32 **
- jika Ble mengirim 'string' = 'LAMP_ON' maka 'PIN_LAMP', HIGH  
- jika Ble mengirim 'string' = 'LAMP_OFF' maka 'PIN_LAMP', LOW

*** saya pikir perlu dibuat "pin_config" terpisah 
- pin definisi:
// untuk led kunci standar 
- PIN_ACC     25
- PIN_IG      26
- PIN_STARTER 27

// untuk led central lock, led Connect, hazzard * led Pesawat
- LED_PIN       2   .// Sebagai led signal jika bluetooth terkoneksi ke Applikasi HP
- PIN_LOCK      4
- PIN_UNLOCK    16
- PIN_HAZZARD   17
- PIN_PESAWAT   23  .// Prilakunya berkedip (high 300ms, low 3000ms) menyala secara kontinyu, 
    saat  pintu terkunci / "locked" = true.

// untuk remote Control 433mhz (RX580) pinMode(INPUT)
- LEDIN_ACC  = 34   .// saat input high, PIN_ACC HIGH ('accOn)
- LEDIN_IG  = 35    .// saat input high, PIN_IG HIGH  ('igOn')
- LEDIN_STARTER  = 36  .// saat input high, PIN_STARTER high 1000, low ('starterOn')
- LEDIN_ALARM  = 39    .// saat input high, ('alarmOn')

Supported BLE commands (notifications sent back as ACK/IGNORED/Locked/Unlocked):
- "Lock" / "Unlock" : pulse lock solenoid (600ms) and flash hazard
- "ACC_ON" / "ACC_OFF" : set ACC output
- "IG_ON" / "IG_OFF" : set IG output
- "STARTER_ON" : pulse starter output for 1000ms
- "ALARM_ON" / "ALARM_OFF" : start/stop alarm blink (200ms on/off)
- "LAMP_ON" / "LAMP_OFF" : set lamp output

Note: `PIN_ALARM` default is 33 (change in include/pin_config.h if needed).

Additional features:
- `PIN_PESAWAT` (13): when `locked == true`, `pesawatOn` = true and the pin blinks: HIGH 300ms, LOW 3000ms continuously.
- Daily warm-up: at 08:00 local RTC time the board performs `warmOn` sequence: `IG_ON`, wait 1000ms, `STARTER_ON` (1s pulse) and keeps systems on for 8 minutes, then turns off ACC/IG/STARTER/LAMP/ALARM (pesawat is left per lock state).

Serial test commands (send via USB serial):
- `rtc` : prints current RTC timestamp
- `i2cscan` : performs an I2C bus scan and prints found addresses
- `warm` : force the warm-up routine immediately (useful for testing)
- `setrtc now` or `setrtc YYYY-MM-DD HH:MM:SS` : set RTC time
- `lock` / `unlock` : trigger lock/unlock pulses for testing (same behavior as BLE commands)
- `warmlen [minutes]` : set or query warm-up duration (default 10 minutes). Value is persisted across reboots.

//untuk RTC 3231 pin 
- SDA_PIN = 21;
- SCL_PIN = 22; 