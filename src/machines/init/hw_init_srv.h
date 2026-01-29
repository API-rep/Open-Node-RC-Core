/*!****************************************************************************
 * @file hw_init_srv.h
 * @brief Servos initialisation script
 * This section contain scripts an routines used to parse servo config file and
 * initialize devices.
 * By this it :
 * - Initialize and allocate servlos ojects pointer
 * - Parse config file and fill child's unset value from parent's one
 * - Create harware device object and initalize them with config file value
 * - Initialize servos defined for the machine configuration
 * 
* This script MUST be include in hw_init.h top file
 *******************************************************************************/// 
 #pragma once

#include <struct.h>
#include <const.h>
#include <macro.h>

#include <config/config.h>

#include <ESP32_PWM_Servo.h>

/**
 * @brief Servo input devices configuration
 * - Build coherent naming access to board servo device structure 
 * - Instance DC-driver devices structure
 * - Execute some sanitary checks 
 * - Create an array of DC driver devices objects
 */

  // 3. Check motherboard servo outputs capacity -> change with serial.print + boardCfg.srvCount
// static_assert (NUM_OF_SRV_DEV <= MAX_ONBOARD_SRV_OUT, "Too much servo outputs configured for the motherboard. Check board.h and machines.h to fix the problem.");

  // 4. Create an array of DC driver devices objects (at least one undefined object)
  //inline ESP32_PWM_Servo srv[NUM_OF_SRV_DEV];

  // 5. DC driver initialisation fuction prototype


void servoInit();