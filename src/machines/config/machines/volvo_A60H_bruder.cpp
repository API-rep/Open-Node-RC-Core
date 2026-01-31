#include "volvo_A60H_bruder.h"


DcDevice dcDevArray[DC_DRV_COUNT] = {

  { 
    .ID = STEERING,
    .infoName = "steering actuators",
    .drvPort = &drvPortArray[DRV_PORT_1A],
    .DevType = DcDevType::DC_MOTOR,
    .usage = DevUsage::GEN_ACTUATOR,
    .mode = DcDrvMode::TWO_WAY_NEUTRAL_CENTER,
    .comChannel = STEERING_BUS, // naming from enum in core/config/combus.h
    .pwmFreq = M_DEF_PWM_FREQ, 
    .polInv = true
  },

  {
    .ID = CABIN_LEFT_MOTOR,
    .infoName = "cabin left motor",
    .drvPort = &drvPortArray[DRV_PORT_1B],
    .DevType = DcDevType::DC_MOTOR,
    .usage = DevUsage::GEN_WHEEL,
    .mode = DcDrvMode::TWO_WAY_NEUTRAL_CENTER,
    .comChannel = DRIVE_SPEED_BUS,
    .pwmFreq = M_DEF_PWM_FREQ, 
    .polInv = true
  },
  
  {
    .ID = CABIN_RIGHT_MOTOR,
    .infoName = "cabin right motor",
    .drvPort = &drvPortArray[DRV_PORT_2B],
    .polInv = false,
    .parentID = CABIN_LEFT_MOTOR
  },
 
  {
    .ID = TRAILER_FRONT_LEFT_MOTOR,
    .infoName = "trailer front left motor",
    .drvPort = &drvPortArray[DRV_PORT_3A],
    .polInv = false,
    .parentID = CABIN_LEFT_MOTOR
  },

  {
    .ID = TRAILER_FRONT_RIGHT_MOTOR,
    .infoName = "trailer front right motor",
    .drvPort = &drvPortArray[DRV_PORT_4A],
    .polInv = true,
    .parentID = CABIN_LEFT_MOTOR
  },

  {
    .ID =TRAILER_REAR_LEFT_MOTOR,
    .infoName = "trailer rear left motor",
    .drvPort = &drvPortArray[DRV_PORT_3B],
    .polInv = true,
    .parentID = CABIN_LEFT_MOTOR
  },

  {
    .ID =TRAILER_REAR_RIGHT_MOTOR,
    .infoName = "trailer rear right motor",
    .drvPort = &drvPortArray[DRV_PORT_4B],
    .polInv = false,
    .parentID = CABIN_LEFT_MOTOR
  },

  {
    .ID =DUMP_ACTUATOR,
    .infoName = "dump actuators L+R",
    .drvPort = &drvPortArray[DRV_PORT_2A],
    .comChannel = DUMP_BUS,
    .polInv = false,
    .parentID = STEERING
  },
};


SrvDevice SrvDevArray[SRV_COUNT] = {

//  TEMPLATE   // steering servo
//  TEMPLATE { 
//  TEMPLATE     .infoName = "steering servo",
//  TEMPLATE     .srvPort = &srvPortArray[SRV_PORT_1A],
//  TEMPLATE     .type = SrvDevType::SERVO,
//  TEMPLATE     .usage = DevUsage::GEN_ACTUATOR,
//  TEMPLATE     .mode = TWO_WAY_NEUTRAL_CENTER,
//  TEMPLATE     .pwmPin = SRV_1_PIN,
//  TEMPLATE     .comChannel = A_CH1,
//  TEMPLATE     .pwmFreq = SRV_DEF_PWM_FREQ, 
//  TEMPLATE     .isInverted = true,
//  TEMPLATE     .minAngleLimit = -90.00,
//  TEMPLATE     .maxAngleLimit = 90.00,
//  TEMPLATE     .zeroAtHwAngle = 0
//  TEMPLATE },
};