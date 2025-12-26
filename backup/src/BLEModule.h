#pragma once

#include <Arduino.h>
#include <functional>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

class BLEModule {
public:
  using WriteHandler = std::function<void(const std::string&)>;
  using ConnHandler = std::function<void(bool)>;

  BLEModule();
  void begin(const char* deviceName, WriteHandler onWrite, ConnHandler onConn);
  void notify(const std::string& value);
  bool connected();

private:
  BLEServer* pServer = nullptr;
  BLECharacteristic* pCharacteristic = nullptr;
  BLEAdvertising* pAdvertising = nullptr; // ⬅️ INI WAJIB

  WriteHandler writeHandler;
  ConnHandler connHandler;

  class ServerCallbacks : public BLEServerCallbacks {
  public:
    ServerCallbacks(BLEModule* parent) : parent(parent) {}
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
  private:
    BLEModule* parent;
  };

  class CharCallbacks : public BLECharacteristicCallbacks {
  public:
    CharCallbacks(BLEModule* parent) : parent(parent) {}
    void onWrite(BLECharacteristic* pChar) override;
  private:
    BLEModule* parent;
  };
};
