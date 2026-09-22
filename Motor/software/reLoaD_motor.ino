#include <Arduino.h>
#include <SimpleFOC.h>

//BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* cimoCharacteristic = NULL;
BLECharacteristic* comiCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

int lastMode = 0;

#define SERVICE_UUID "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CIMO_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define COMI_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"

MagneticSensorSPI sensor = MagneticSensorSPI(10, 14, 0x3FFF);
BLDCMotor motor = BLDCMotor(7);
BLDCDriver6PWM driver = BLDCDriver6PWM(4, 7, 5, 8, 6, 9);
Commander command = Commander(Serial);

void doTarget(char* cmd) { command.scalar(&motor.target, cmd); }
void doMotor(char* cmd) { command.motor(&motor, cmd); }

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { deviceConnected = true; };
  void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* comiCharacteristic) {
    std::string value = comiCharacteristic->getValue();
    if (value.length() > 0) {
      
      std::string rpmVariable = "rpm ";
      std::string degVariable = "deg ";

      // --- RPM MODE ---
      if (value.compare(0, rpmVariable.size(), rpmVariable) == 0) {
        value = value.substr(rpmVariable.size());
        int rpmValue = atoi(value.c_str());
        float radValue = rpmValue / 9.5493;

        if(motor.controller != MotionControlType::velocity){
             motor.controller = MotionControlType::velocity;
             motor.LPF_velocity.Tf = 0.005f;
             lastMode = 0;
        }
        motor.target = radValue;
      }

      // --- POSITION MODE ---
      if (value.compare(0, degVariable.size(), degVariable) == 0) {
        value = value.substr(degVariable.size());
        int degValue = atoi(value.c_str());
        float targetRad = degValue / 57.2958;
        
        if(lastMode == 0){
            motor.controller = MotionControlType::angle;
            motor.LPF_velocity.Tf = 0.0005f;
            lastMode = 1;
        }

        // --- SHORTEST PATH LOGIC (The Fix) ---
        // This replicates your old behavior (nearest angle) without breaking commutation.
        
        // 1. Normalize target to 0-2PI
        float target_mod = fmod(targetRad, _2PI);
        if (target_mod < 0) target_mod += _2PI;

        // 2. Normalize current motor angle to 0-2PI
        float current_mod = fmod(motor.shaft_angle, _2PI);
        if (current_mod < 0) current_mod += _2PI;

        // 3. Calculate the shortest difference
        float error = target_mod - current_mod;
        
        // If difference is > 180 deg, go the other way
        if (error > PI) error -= _2PI;
        if (error < -PI) error += _2PI;

        // 4. Apply to the real absolute angle
        motor.target = motor.shaft_angle + error;
      }
    }
  }
};

void setup() {
  Serial.begin(115200);

  // BLE Setup
  BLEDevice::init("Motor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);
  cimoCharacteristic = pService->createCharacteristic(
    CIMO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
  comiCharacteristic = pService->createCharacteristic(
    COMI_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE);
  comiCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  cimoCharacteristic->addDescriptor(new BLE2902());
  comiCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  // FOC Setup
  sensor.init();
  motor.linkSensor(&sensor);

  driver.voltage_power_supply = 12;
  driver.dead_zone = 0.02f; // Keeps MOSFETs safe
  driver.init();
  motor.linkDriver(&driver);

  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
  motor.KV_rating = 750; 
  
  motor.voltage_limit = 4.0;
  // Correct Resistance
  //motor.phase_resistance = 0.1; 

  motor.voltage_sensor_align = 0.65; 
  motor.torque_controller = TorqueControlType::voltage; 
  motor.controller = MotionControlType::velocity;

  // PID Velocity
  motor.PID_velocity.P = 0.0005f; 
  motor.PID_velocity.I = 0.005f; 
  motor.PID_velocity.D = 0;
  motor.PID_velocity.output_ramp = 10;
  motor.LPF_velocity.Tf = 0.01f; 

  // PID Angle
  motor.P_angle.P = 10; // Lower P for 0.1ohm motor
  motor.P_angle.I = 0;
  motor.P_angle.output_ramp = 300;

  // Limits
  motor.current_limit = 3.0f; 
  motor.velocity_limit = 10; 


  motor.monitor_downsample = 500;
  motor.useMonitoring(Serial);
  motor.init();
  motor.initFOC();
  
  motor.target = 0;

  command.add('T', doTarget, "target angle");
  command.add('M', doMotor, "my motor motion");
}

void loop() {
  motor.loopFOC();
  motor.move();
  //motor.monitor();
  command.run();

  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
}