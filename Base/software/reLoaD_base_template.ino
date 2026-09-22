/*
 * reLoaD Base software template
 *
 * Provides the module-level setup shared by Base applications:
 *   - BLE command and notification characteristics
 *   - Wireless power transfer (WPT) PWM control
 *
 * Add application-specific sensors, commands, and control logic where marked.
 */

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

constexpr uint8_t STATUS_LED_PIN = 2;
constexpr uint8_t WPT_PIN = 18;
constexpr uint8_t WPT_PWM_CHANNEL = 0;
constexpr uint8_t WPT_PWM_RESOLUTION_BITS = 4;
constexpr uint8_t WPT_PWM_DUTY = 7;

constexpr char BLE_DEVICE_NAME[] = "reLoaD Base";
constexpr char BLE_SERVICE_UUID[] = "19b10000-e8f2-537e-4f6c-d104768a1214";
constexpr char BLE_NOTIFY_UUID[] = "19b10001-e8f2-537e-4f6c-d104768a1214";
constexpr char BLE_COMMAND_UUID[] = "19b10002-e8f2-537e-4f6c-d104768a1214";

BLEServer *bleServer = nullptr;
BLECharacteristic *bleNotifyCharacteristic = nullptr;
BLECharacteristic *bleCommandCharacteristic = nullptr;

bool bleConnected = false;
bool wasBleConnected = false;

void stopWirelessPowerTransfer() {
  ledcWrite(WPT_PWM_CHANNEL, 0);
  ledcDetachPin(WPT_PIN);
  digitalWrite(WPT_PIN, LOW);
  digitalWrite(STATUS_LED_PIN, LOW);
}

void startWirelessPowerTransfer(uint32_t frequencyHz) {
  if (frequencyHz == 0) {
    stopWirelessPowerTransfer();
    return;
  }

  ledcSetup(WPT_PWM_CHANNEL, frequencyHz, WPT_PWM_RESOLUTION_BITS);
  ledcAttachPin(WPT_PIN, WPT_PWM_CHANNEL);
  ledcWrite(WPT_PWM_CHANNEL, WPT_PWM_DUTY);
  digitalWrite(STATUS_LED_PIN, HIGH);
}

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
  // WPT can be controlled with startWirelessPowerTransfer(frequencyHz) and
  // stopWirelessPowerTransfer().
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

  pinMode(WPT_PIN, OUTPUT);
  digitalWrite(WPT_PIN, LOW);

  initializeBle();

  // TODO: Initialize application-specific hardware here.
}

void loop() {
  maintainBleConnection();

  // TODO: Add non-blocking application logic here.
}
