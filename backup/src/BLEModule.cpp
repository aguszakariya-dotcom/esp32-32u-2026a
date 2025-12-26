#include "BLEModule.h"
#include <BLE2902.h>

static const char* SERVICE_UUID = "12345678-1234-1234-1234-123456789abc";
static const char* CHAR_UUID    = "abcdefab-1234-5678-1234-abcdefabcdef";

BLEModule::BLEModule() {}

void BLEModule::begin(const char* deviceName, WriteHandler onWrite, ConnHandler onConn) {
  writeHandler = onWrite;
  connHandler = onConn;

  BLEDevice::init(deviceName);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks(this));

  BLEService* pService = pServer->createService(BLEUUID(SERVICE_UUID));

  pCharacteristic = pService->createCharacteristic(
    BLEUUID(CHAR_UUID),
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new CharCallbacks(this));
  pCharacteristic->setValue("READY");

  pService->start();

  // ===== Advertising =====
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLEUUID(SERVICE_UUID));
  pAdvertising->setScanResponse(true);

  // PENTING UNTUK ANDROID / MIT APP INVENTOR
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  pAdvertising->start();
}

void BLEModule::notify(const std::string& value) {
  if (!pCharacteristic || !connected()) return;
  pCharacteristic->setValue(value);
  pCharacteristic->notify();
}

bool BLEModule::connected() {
  if (!pServer) return false;
  return pServer->getConnectedCount() > 0;
}

/* ===== ServerCallbacks ===== */

void BLEModule::ServerCallbacks::onConnect(BLEServer* pServer) {
  if (parent && parent->connHandler) {
    parent->connHandler(true);
  }
}

void BLEModule::ServerCallbacks::onDisconnect(BLEServer* pServer) {
  if (parent && parent->connHandler) {
    parent->connHandler(false);
  }

  // 🔥 INI KUNCI RECONNECT
  pServer->getAdvertising()->start();
}

/* ===== CharCallbacks ===== */

void BLEModule::CharCallbacks::onWrite(BLECharacteristic* pChar) {
  std::string val = pChar->getValue();
  if (parent && parent->writeHandler) {
    parent->writeHandler(val);
  }
}
