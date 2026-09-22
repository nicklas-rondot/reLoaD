# *reLoaD* framework

The *reLoaD* (reconfigurable Lab on a Disc) framework is an open-hardware centrifugal microfluidics framework for research, teaching and application. It consists of three core modules:

1. The *Motor* module:
  Consists of a consumer-grade brushless DC (BLDC) motor driven by a PCB with a microcontroller, three-phase half bridge and magnetic encoder using field-oriented-control (FOC). This enables both high-speed rotation for centrifugal microfluidic operations and precise angular positioning for sensor alignment.
2. The *Base* module:
  A stationary PCB with a microcontroller that serves as a platform to mount/control stationary sensors and actuators. Furthermore, it integrates wireless power transmission electronics.
3. The *Spinnner* module:
  A PCB co-rotating with the microfluidic disc with a microcontroller that serves as a platform to mount/control co-rotating sensors and actuators (primarily heaters). It integrates rectification and power regulation electronics for wirelessly receiving power from the *Base* module.

All modules are based on the ESP32-S3 microcontroller. Sensors and actuators can be placed on *Shields*, PCBs that mount to the Base and Spinner modules using standard pin headers. It is advantageous to place optical sensors and actuators on a *Base Shield*, as this allows using the rotation of the microfluidic disc to align multiple chambers with a single sensor one after another. Moreover, it is recommended to place sensors and actuators that need to be in direct contact with the microfluidic disc on the *Spinner Shield*, since this enables interaction with the disc during rotation and necessitates no movable components. The control software is available at github.com/nicklas-rondot/reLoaD-control