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
uint32_t value = 0;

float initial_zero_angle = 0;
float initial_offset = -0.545; // previously -0.225
float fw_dir_offset = 0;

int lastMode = 0;

#define SERVICE_UUID "19b10000-e8f2-537e-4f6c-d104768a1214"
#define CIMO_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define COMI_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"


// MagneticSensorSPI(int cs, float _cpr, int _angle_register)
MagneticSensorSPI sensor = MagneticSensorSPI(10, 14, 0x3FFF);

// BLDC motor & driver instance
BLDCMotor motor = BLDCMotor(7);
BLDCDriver6PWM driver = BLDCDriver6PWM(4, 7, 5, 8, 6, 9);

// Commander interface constructor
Commander command = Commander(Serial);

void doTarget(char* cmd) {
  command.scalar(&motor.target, cmd);
}
void doMotor(char* cmd) {
  command.motor(&motor, cmd);
}


class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};



class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* comiCharacteristic) {
    std::string value = comiCharacteristic->getValue();
    if (value.length() > 0) {

      //Serial.print("Characteristic event, written: ");
      //Serial.println(value.c_str()); // Print the integer value

      std::string rpmVariable = "rpm ";
      std::string degVariable = "deg ";

      if (value.compare(0, rpmVariable.size(), rpmVariable) == 0) {
        value = value.substr(rpmVariable.size());
        int rpmValue = atoi(value.c_str());
        //Serial.println(rpmValue);
        float radValue = rpmValue / 9.5493;

        motor.controller = MotionControlType::velocity;
        motor.LPF_velocity.Tf = 0.005f;
        lastMode = 0;

        // Field weakening to achieve higher RPM
        if(rpmValue > 0){
          motor.zero_electric_angle = initial_zero_angle + initial_offset - fw_dir_offset;
        }
        if(rpmValue < 0){
          motor.zero_electric_angle = initial_zero_angle + initial_offset + fw_dir_offset;
        }

        if(rpmValue == 0){
          motor.zero_electric_angle = initial_zero_angle + initial_offset;
        }

        _delay(10);

        motor.move(radValue);
      }

      if (value.compare(0, degVariable.size(), degVariable) == 0) {
        value = value.substr(degVariable.size());
        int degValue = atoi(value.c_str());
        //Serial.println(degValue);
        float radValue = degValue / 57.2958;
        float angle = sensor.getAngle();
        if(lastMode == 0){
          float normalizedAngle = fmod(angle, 2 * PI);
          motor.controller = MotionControlType::angle;
          motor.LPF_velocity.Tf = 0.0005f; // added one zero
          motor.zero_electric_angle = initial_zero_angle + initial_offset;
          motor.sensor_offset = normalizedAngle - angle;  //+/- the expression depends on orientation of magnet
        }
        motor.move(radValue);
        lastMode = 1;
      }
    }
  }
};


void setup() {

  Serial.begin(115200);
  delay(100);

  //BLE------------
  // Create the BLE Device
  BLEDevice::init("Motor");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  cimoCharacteristic = pService->createCharacteristic(
    CIMO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);

  // Create the ON button Characteristic
  comiCharacteristic = pService->createCharacteristic(
    COMI_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE);

  // Register the callback for the ON button characteristic
  comiCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  cimoCharacteristic->addDescriptor(new BLE2902());
  comiCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");

  //FOC----------

  sensor.init();
  motor.linkSensor(&sensor);

  // driver config
  driver.voltage_power_supply = 12;
  driver.init();


  // link the motor and the driver
  motor.linkDriver(&driver);

  // limiting motor movements
  //motor.phase_resistance = 0.251; // [Ohm]
  //motor.phase_inductance = 0.0001; // [Ohm]
  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
  //motor.current_limit = 10;   // [Amps] - if phase resistance defined
  // motor.voltage_limit = 1;   // [V] - if phase resistance not defined


  motor.KV_rating = 360;
  motor.phase_resistance = 0.24;

  motor.voltage_sensor_align = 0.6;

  // set torque mode to be used
  motor.torque_controller = TorqueControlType::voltage;  // ( default )
  // motor.torque_controller = TorqueControlType::dc_current;
  // motor.torque_controller = TorqueControlType::foc_current;

  // set motion control loop to be used
  // motor.controller = MotionControlType::torque; //      - torque control
  motor.controller = MotionControlType::velocity;  //    - velocity motion control
                                                   // motor.controller = MotionControlType::angle; //       - position/angle motion control
                                                   // motor.controller = MotionControlType::velocity_openloop; //    - velocity open-loop control
                                                   // motor.controller = MotionControlType::angle_openloop; //       - position open-loop control

  motor.PID_velocity.P = 1.5f; // Had it on 6 before for velocity tests but then at low speeds strong rumbling
  motor.PID_velocity.I = 0.05f;  // if higher, rumbling at high speeds will be stronger
  motor.PID_velocity.D = 0;
  motor.PID_velocity.output_ramp = 800;

  motor.LPF_velocity.Tf = 0.005f;

  motor.P_angle.output_ramp = 300;  // 900 no load
  motor.P_angle.P = 20;             //27.5 no load. Had it on 180 for position tests but may lead to rumbing. must fit to lpf filter 
  motor.P_angle.I = 0;
  motor.velocity_limit = 10;  // [rad/s]

  //motor.voltage_limit = 7;
  motor.current_limit = 5;

  motor.useMonitoring(Serial);

  // init motor hardware
  motor.init();


  // align sensor and start FOC
  motor.initFOC();

  _delay(100);

  initial_zero_angle = motor.zero_electric_angle;
  motor.zero_electric_angle += initial_offset;

  _delay(100);

  // set the initial motor target
  motor.target = 0;  // unit depends on control mode

  // add target command T
  command.add('T', doTarget, "target angle");
  command.add('M', doMotor, "my motor motion");
  _delay(100);
}

void loop() {

  motor.loopFOC();
  motor.move();
  motor.monitor();
  command.run();

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    Serial.println("Device disconnected.");
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
    Serial.println("Device Connected");
  }
}