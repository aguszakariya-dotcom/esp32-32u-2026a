#include <Arduino.h>

// Simple BLE peripheral example for ESP32
// Prints: device address (MAC), service UUID, characteristic UUID
// Use these values in MIT App Inventor's BluetoothLE extension

#include <BluetoothSerial.h>
#include <esp_bt.h>
#include <esp_system.h>

// Change these UUIDs as needed (use full 128-bit UUIDs for App Inventor)

// Serial baud: default 115200. You can change at runtime by typing `b9600` or
// `b115200` on the Serial Console within the first 3 seconds after reset.
static const uint32_t DEFAULT_BAUD = 115200;
static uint32_t serialBaud = DEFAULT_BAUD;

// LED pin turned on when a BLE central connects. Change if your board uses
// a different built-in LED pin.
static const int LED_PIN = 2;

BluetoothSerial SerialBT;
bool wasConnected = false;

void setup() {
  Serial.begin(serialBaud);
  delay(10);
  Serial.println("Starting BLE peripheral...");

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

  Serial.print("Using serial baud: "); Serial.println(serialBaud);

  // Initialize Classic Bluetooth SPP (Serial Port Profile)
  if (!SerialBT.begin("ESP32-BT-Mobile")) {
    Serial.println("Failed to start Classic Bluetooth");
  } else {
    Serial.println("Bluetooth SPP started as 'ESP32-BT-Mobile'");
  }

  // Print the device MAC address (useful for pairing in some apps)
  uint8_t btMac[6];
  if (esp_read_mac(btMac, ESP_MAC_BT) == ESP_OK) {
    char macBuf[18];
    sprintf(macBuf, "%02X:%02X:%02X:%02X:%02X:%02X", btMac[0], btMac[1], btMac[2], btMac[3], btMac[4], btMac[5]);
    Serial.println("--- Bluetooth Info (use these in App Inventor for Classic BT) ---");
    Serial.print("Device Name: "); Serial.println("ESP32-BT-Mobile");
    Serial.print("Device Address (MAC): "); Serial.println(macBuf);
    Serial.println("-------------------------------------------------------------");
  }
  // Print initial connection status once to avoid flooding
  wasConnected = SerialBT.connected();
  Serial.print("BT connected: "); Serial.println(wasConnected ? "Yes" : "no");
}

void loop() {
  // Only print when connection status changes (prevents flooding)
  bool connected = SerialBT.connected();
  if (connected != wasConnected) {
    if (connected) {
      digitalWrite(LED_PIN, HIGH);
      Serial.print("BT connected: "); Serial.println("Yes");
      SerialBT.println("BT:CONNECTED");
    } else {
      digitalWrite(LED_PIN, LOW);
      Serial.print("BT connected: "); Serial.println("no");
      SerialBT.println("BT:DISCONNECTED");
    }
    wasConnected = connected;
  }

  // Echo data from BT to Serial console (optional convenience)
  while (SerialBT.available()) {
    int c = SerialBT.read();
    Serial.write(c);
  }
}

/*
Optional: Central (scanner) flow to discover devices and read services/characteristics:
- Use BLEDevice::getScan() and a custom callback to list found devices and addresses.
- To enumerate characteristics you must connect to the device and call getServices()/getCharacteristics().
This example focuses on the peripheral side because MIT App Inventor typically connects to a peripheral device.
*/