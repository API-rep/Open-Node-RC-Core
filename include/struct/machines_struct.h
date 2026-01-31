/*!****************************************************************************
 * @file  machines_struct.h
 * @brief machines structure and variable definition file
 * This file contain all structure used to define machine configuration and features.
 *******************************************************************************/// 
#pragma once

#include <stdint.h>
#include <optional>

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

/**
 * @brief DC driver module structure
 * Contain configuration of DC driver module feature
 */

typedef struct {
  const char* infoName;
  const InputPinMode sleepPinMode = InputPinMode::UNDEFINED;  // driver sleep mode (ACTIVE_LOW/HIGH, UNDEFINED if not available)
  const InputPinMode brakePinMode = InputPinMode::UNDEFINED;  // driver brake mode (ACTIVE_LOW/HIGH, UNDEFINED if not available)
  const InputPinMode faultPinMode = InputPinMode::UNDEFINED;  // driver fault mode (ACTIVE_LOW/HIGH, OPEN_DRAIN, UNDEFINED if not available)
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
  std::optional<uint8_t> pwmPin;
  std::optional<uint8_t> brkPin;
  std::optional<uint8_t> enPin;
  std::optional<uint8_t> slpPin;
  std::optional<uint8_t> fltPin;
} DriverPort;


/**
 * @brief Board servo ports structure
 * Contain all configuration and manipulators of board servo ports
 */

 typedef struct {
  const int8_t ID;
  const char* infoName;
  std::optional<uint8_t> pwmPin;
  std::optional<uint8_t> sensePin;
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
  uint8_t drvPortCount;                         // number of DC driver device configured
  ServoPort* srvPort;                           // DC driver device config structure
  uint8_t srvPortCount;                         // number of servo device configured
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
  std::optional<uint8_t> comChannel;          // internal com-bus channel used to set the driver speed
  std::optional<uint32_t> pwmFreq;            // driver PWM frequency (in hz)
  bool polInv = false;                        // driver polarity inversion (true = inverted)
  std::optional<float> maxFwSpeed;            // maximum forward speed (0 to 100% - Defaut 100%)
  std::optional<float> maxBackSpeed;          // maximum backward speed (0 to 100% - Defaut 100%)
  const std::optional<uint8_t> parentID;      // parent identifier (used in clone mode)
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
  std::optional<uint8_t> usage;             // device usage in the vehicle
  std::optional<uint8_t> mode;              // servo configuration
  std::optional<uint8_t> comChannel;        // internal com-bus used to set the servo angle
  std::optional<uint32_t> pwmFreq;          // output PWM frequency (in hz)
  bool isInverted = false;                  // servo sense inversion (true = inverted)
  std::optional<float> minAngleLimit;       // servo minimun angle limit (0 to min HW angle)
  std::optional<float> maxAngleLimit;       // servo maximum angle limit (0 to max HW angle)
  std::optional<float> zeroAtHwAngle;       // servo zero at hardware angle (min HW angle to max HW angle, typ. 0)
  std::optional<float> maxSpeed;            // servo maximum speed
  std::optional<float> maxAccel;            // servo maximum acceleration
  const std::optional<uint8_t> parentID;    // parent identifier (used in clone mode)
} SrvDevice;


/**
 * @brief Machine main structure
 *  * Contain all configuration and manipulators of the machine
 * NOTE:
 * - Instance of these structures is created in config/machines/machine_name.h
 */

typedef struct {
  const char* infoName;                                 // machine name
  CombusLayout combusLayout = CombusLayout::UNDEFINED;  // machine com-bus layout
  std::optional<float> maxFwSpeed;                      // maximum forward driving speed (0 to 100% - Defaut 100%)
  std::optional<float> maxBackSpeed;                     // maximum backward driving speed (0 to 100% - Defaut 100%)
  DcDevice* dcDev;                                      // DC driver device config structure
  uint8_t dcDevCount;                                    // number of DC driver device configured
  SrvDevice* srvDev;                                    // DC driver device config structure
  uint8_t srvDevCount;                                   // number of servo device configured
} Machine;

// EOF machines_struct.h