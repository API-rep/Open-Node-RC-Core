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

#include <core/config/combus/combus.h>

/**
 * @brief Vehicle configuration
 */

#ifndef BOARD 
  #define BOARD         ESP32_8M_6S     // défault vehicle motherboard
#endif

#define MAX_FW_SPEED         100.00     // maximum vehicle forward driving speed (0 to 100.00%)
#define MAX_BACK_SPEED       100.00     // maximum vehicle backward driving speed (0 to 100.00%)

/** @brief motors/servo settings */

#define M_DEF_PWM_FREQ        16000     // motors default PWM frequency (in hz)
#define SRV_DEF_PWM_FREQ         50     // servo default PWM frequency (in hz)
#define COOLING_FAN_SPEED       100     // cooling fan speed (in %)

  // include board definition
#include <machines/config/boards/boards.h>


/**
 * Vehicle devices definition
 * Place here all config of devices drived by the vehicle such as :
 * - Devices wired to DC driver (motor, actuator, ...)
 * - Devices wired to SRV ports (servo ...)
 * - Lights
 * 
 * NOTE:
 * - In device control loop, use DC_DRv_COUNT/SRV_COUNT to loop through all devices
 *   and avoid zero size array issue.
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



  // servos index
 enum SrvDev { // STEERING = 0
               SRV_COUNT
             };

   // Servo config structure declaration. Edit data in cpp file
 extern SrvDevice SrvDevArray[];
 


  /** @brief Vehicle config structure definition */

inline constexpr Machine machine {
  .infoName = "Volvo A60H Bruder",
  .combusLayout = CombusLayout::DUMPER_TRUCK,
  .dcDev = dcDevArray,
  .dcDevCount = DC_DRV_COUNT,
  .srvDev = SrvDevArray,  // set to nullptr if no servo used or srvDevArray if servo used
  .srvDevCount = SRV_COUNT
};

// EOF volvo_A60H_bruder.h