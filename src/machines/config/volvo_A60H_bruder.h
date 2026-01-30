/*!****************************************************************************
 * @file  volvo_A60H_bruder.h
 * @brief Volvo_A60H_Bruder conversion
 * Configuration file for the API Brudder Volvo_A60H full electric conversion
 * - 6 motors drivers used for traction (one per wheel)
 * - 1 motor driver used for steering actuator (one or two wired in //)
 * - 1 motor driver used for 2 dump actuators wired in //
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

#include <core/config/config.h>
#include <machines/config.h>

/**
 * @brief Vehicle configuration
 */

#define MACHINE_INFO_NAME  "Volvo A60H Bruder"

#ifndef BOARD 
  #define BOARD              ESP32_8M_6S       // défault vehicle motherboard
#endif

#define MAX_FW_SPEED       100.00              // maximum vehicle forward driving speed (0 to 100.00%)
#define MAX_BACK_SPEED     100.00              // maximum vehicle backward driving speed (0 to 100.00%)

#define DEFAULT_SERVO_SPEED    NOT_SET   // default servo speed
#define DEFAULT_SERVO_ACCEL    NOT_SET   // default servo speed


#include "boards/boards.h"

/**
 * Vehicle devices definition
 * Place here all config of devices drived by the vehicle such as :
 * - Devices wired to DC driver (motor, actuator, ...)
 * - Devices wired to SRV ports (servo ...)
 * - Lights
 * 
 * NOTE:
 * - If multiple motors are wired on se same driver output, configure only one MOTOR_x section for the group
 * - enum entry and dcDevArray/srvDevArray MUST have the same order
 * - if a entry is not used :
 *   -> set  .dcDrvDev = nullptr  and/or  .srvDev = nullptr
 *   -> set  dcDrvDevCount = 0  and/or  srvDevCount = 0
 */

  // DC drivers index
enum DrvDev { STEERING = 0,
              CABIN_LEFT_MOTOR,
              CABIN_RIGHT_MOTOR,
              TRAILER_FRONT_LEFT_MOTOR,
              TRAILER_FRONT_RIGHT_MOTOR,
              TRAILER_REAR_LEFT_MOTOR,
              TRAILER_REAR_RIGHT_MOTOR,
              DUMP_ACTUATOR, 
              DC_DRV_COUNT};

  // DC driver config structure declaration. Edit data in cpp file
extern DcDevice dcDevArray[DC_DRV_COUNT];


// uncomment for servo configuration
//    // DC driver config structure declaration. Edit data in cpp file
//  extern ServoPort SrvDevArray[];
//  
//  
//  
//    // servos index
//  enum SrvDev { STEERING = 0
//         // ANOTHER SERVO
//              };
//  
//  



/**
 * @brief Vehicle structure definition
 * Place here all vehicle configuration contants
 * NOTE:
 * -If no ServoPort and/or drvDevConfig is need, place .xxxDevCount to 0 and xxxDevConfig to nullptr
 */
inline constexpr Machine machine {
  .infoName = "Volvo A60H Bruder",
  .dcDev = dcDevArray,
  .dcDevCount = DC_DRV_COUNT,
  .srvDev = nullptr,
  .srvDevCount = 0
//  .srvDev = srvDevArray,
//  .srvDevCount = sizeof(srvDevArray) / sizeof(srvDevArray[0])
};

// EOF volvo_A60H_bruder.h