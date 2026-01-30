/*!****************************************************************************
 * @file  machines_struct.h
 * @brief machines structure and variable definition file
 * This file contain all structure used to define machine configuration and features.
 *******************************************************************************/// 
#pragma once

#include <stdint.h>

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

/**
 * @brief DC driver module structure
 * Contain configuration of DC driver module feature
 */

typedef struct {
  const char* infoName;
  const InputPinMode sleepPinMode = InputPinMode::UNDEFINED;  // driver sleep mode (ACTIVE_LOW/HIGH, NOT_SET if not available)
  const InputPinMode brakePinMode = InputPinMode::UNDEFINED;  // driver brake mode (ACTIVE_LOW/HIGH, NOT_SET if not available)
  const InputPinMode faultPinMode = InputPinMode::UNDEFINED;  // driver fault mode (ACTIVE_LOW/HIGH, OPEN_DRAIN, NOT_SET if not available)
} DriverModel;



/**
 * @brief Board DC driver ports structure
 * Contain all configuration and manipulators of board DC driver ports
 * NOTE:
 * - "driverModel" define DC driver host by the port. For builtin (soldered on board)
 *   specify model type in src/config/machines/boards/your_board.h file (ex: .driverModel = &DRV8874).
 *   Otherwise, leave to "nullptr" (default) and configure driver model in config.h file (or -D compilation command line)
 */

typedef struct {
  const int8_t ID;
  const char* infoName;
  const DriverModel* driverModel = nullptr;
  int8_t pwmPin = NOT_SET;
  int8_t brkPin = NOT_SET;
  int8_t enPin = NOT_SET;
  int8_t slpPin = NOT_SET;
  int8_t fltPin = NOT_SET;
} DriverPort;


/**
 * @brief Board servo ports structure
 * Contain all configuration and manipulators of board servo ports
 */

 typedef struct {
  const int8_t ID;
  const char* infoName;
  int8_t pwmPin = NOT_SET;
  int8_t sensePin = NOT_SET;
} ServoPort;


/**
 * @brief Board main structure
 * NOTE:
 * - Instance of these structures is created in src/config/machines/boards/borad_name.h
 */

typedef struct {
  const char* infoName;                         // board name
//  float maxFwSpeed =  PERCENT_MAX;            // maximum forward driving speed (0 to 100% - Defaut 100%)
//  float maxBackSpeed = PERCENT_MAX;           // maximum backward driving speed (0 to 100% - Defaut 100%)
  DriverPort* drvPort;                          // DC driver device config structure
  int8_t drvPortCount = NOT_SET;                // number of DC driver device configured
  ServoPort* srvPort;                           // DC driver device config structure
  int8_t srvPortCount = NOT_SET;                 // number of servo device configured
} Board;





/**
 * @brief DC device structure
 * Contain all configuration and manipulators of devices wired to DC driver ports
 * NOTE:
 * - Instance of these structures are created in hw_init.h
 * - DON'T forget tu update hw_init.h applyParentConfig() section if this structure change
 */

typedef struct {
  const int8_t ID;                            // DC device ID
  const char* infoName;                       // attached device short description
  const DriverPort* drvPort;                  // DC device board driver port
  DcDevType DevType = DcDevType::UNDEFINED;   // attached device type
  DevUsage usage = DevUsage::UNDEFINED;       // attached device usage in the vehicle
  DcDrvMode mode = DcDrvMode::UNDEFINED;      // DC device configuration
  int8_t comChannel = NOT_SET;                // internal com-bus channel used to set the driver speed
  uint32_t pwmFreq = NOT_SET;                 // driver PWM frequency (in hz)
  bool polInv = false;                        // driver polarity inversion (true = inverted)
  float maxFwSpeed = PERCENT_MAX;             // maximum forward speed (0 to 100% - Defaut 100%)
  float maxBackSpeed = PERCENT_MAX;           // maximum backward speed (0 to 100% - Defaut 100%)
  const int8_t parentID = NOT_SET;            // parent identifier (used in clone mode)
} DcDevice;


/**
 * @brief Servo device structure
 * Contain all configuration and manipulators of devices wired to servo driver ports
 * NOTE:
 * - Instance of these structures are created in hw_init.h
 * - DON'T forget tu update hw_init.h FILL_DRV section if this structures change
 */

typedef struct {
  const int8_t ID;                          // servo ID
  const char* infoName;                     // device short description
  const ServoPort* srvPort;                 // DC device board driver port
  SrvDevType type = SrvDevType::UNDEFINED;  // device wired to servo output
  int8_t usage = NOT_SET;                   // device usage in the vehicle
  int8_t mode = NOT_SET;                    // servo configuration
  int8_t comChannel = NOT_SET;              // internal com-bus used to set the servo angle
  uint32_t pwmFreq = NOT_SET;               // output PWM frequency (in hz)
  bool isInverted = false;                  // servo sense inversion (true = inverted)
  float minAngleLimit = NOT_SET;            // servo minimun angle limit (0 to min HW angle)
  float maxAngleLimit = NOT_SET;            // servo maximum angle limit (0 to max HW angle)
  float zeroAtHwAngle = 0;                  // servo zero at hardware angle (min HW angle to max HW angle, typ. 0)
  float maxSpeed = NOT_SET;                 // servo maximum speed
  float maxAccel = NOT_SET;                 // servo maximum acceleration
  const int8_t parentID = NOT_SET;          // parent identifier (used in clone mode)
} SrvDevice;


/**
 * @brief Machine main structure
 *  * Contain all configuration and manipulators of the machine
 * NOTE:
 * - Instance of these structures is created in config/machines/machine_name.h
 */

typedef struct {
  const char* infoName;                         // machine name
  float maxFwSpeed =  PERCENT_MAX;              // maximum forward driving speed (0 to 100% - Defaut 100%)
  float maxBackSpeed = PERCENT_MAX;             // maximum backward driving speed (0 to 100% - Defaut 100%)
  DcDevice* dcDev;                              // DC driver device config structure
  int8_t dcDevCount = NOT_SET;                  // number of DC driver device configured
  SrvDevice* srvDev;                            // DC driver device config structure
  int8_t srvDevCount = NOT_SET;                 // number of servo device configured
} Machine;

// EOF machines_struct.h