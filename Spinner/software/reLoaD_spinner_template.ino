/*
 * reLoaD Spinner software template
 *
 * Provides the BLE command and notification interface shared by Spinner
 * applications. Add application-specific sensors, actuators, commands, and
 * control logic where marked.
 */

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

constexpr uint8_t STATUS_LED_PIN = 2;

constexpr char BLE_DEVICE_NAME[] = "reLoaD Spinner";
constexpr char BLE_SERVICE_UUID[] = "19b10000-e8f2-537e-4f6c-d104768a1214";
constexpr char BLE_NOTIFY_UUID[] = "19b10001-e8f2-537e-4f6c-d104768a1214";
constexpr char BLE_COMMAND_UUID[] = "19b10002-e8f2-537e-4f6c-d104768a1214";

BLEServer *bleServer = nullptr;
BLECharacteristic *bleNotifyCharacteristic = nullptr;
BLECharacteristic *bleCommandCharacteristic = nullptr;

bool bleConnected = false;
bool wasBleConnected = false;

void sendBleMessage(const String &message) {
  if (!bleConnected || bleNotifyCharacteristic == nullptr) {
    return;
  }

  bleNotifyCharacteristic->setValue(message.c_str());
  bleNotifyCharacteristic->notify();
}

void handleBleCommand(const std::string &command) {
  Serial.print("BLE command: ");
  Serial.println(command.c_str());

  // TODO: Parse application-specific commands here.
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    bleConnected = true;
    Serial.println("BLE client connected");
  }

  void onDisconnect(BLEServer *) override {
    bleConnected = false;
    Serial.println("BLE client disconnected");
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    const std::string command = characteristic->getValue();
    if (!command.empty()) {
      handleBleCommand(command);
    }
  }
};

void initializeBle() {
  BLEDevice::init(BLE_DEVICE_NAME);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService *service = bleServer->createService(BLE_SERVICE_UUID);

  bleNotifyCharacteristic = service->createCharacteristic(
      BLE_NOTIFY_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  bleNotifyCharacteristic->addDescriptor(new BLE2902());

  bleCommandCharacteristic = service->createCharacteristic(
      BLE_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE);
  bleCommandCharacteristic->setCallbacks(new CommandCallbacks());
  bleCommandCharacteristic->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->setScanResponse(false);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising started");
}

void maintainBleConnection() {
  if (!bleConnected && wasBleConnected) {
    delay(500);
    bleServer->startAdvertising();
    Serial.println("BLE advertising restarted");
  }

  wasBleConnected = bleConnected;
}

void setup() {
  Serial.begin(115200);

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  initializeBle();

  // TODO: Initialize application-specific hardware here.
}

void loop() {
  maintainBleConnection();

  // TODO: Add non-blocking application logic here.
}
