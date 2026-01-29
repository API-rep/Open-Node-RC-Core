/*!****************************************************************************
 * @file  struct.h
 * @brief structure and variable definition file
 * This file contain the recurent structure and variable used in the projet
 * Share it with other device of the same project to ensure compatibility (trailer, 
 * light module, sound module ...)
 *******************************************************************************/// 
#pragma once

#include <stdint.h>

#include <const.h>
#include <defs/defs.h>

/**
 * @brief Internal communication bus structure
 * This structure is used to standardized communication into main code. It act as a harware
 * abstraction layer between different input modules (RC protocol, bluetooth, wifi ...), the main code 
 * and output modules (IO expander, sound module). Its data structure is devided in channels :
 * - Digital channel to store two state date
 * - Analog channel to store analog/multi state data
 * - A runlevel data to store machine state
 * 
 * Its instance is create in init\sys_config.h file
 * 
 * NOTE:
 * - do not change uint16_t size for AnalogBus. Some system sub value depend of this size
 * - "isDrived" flag have to be set true if its channel is perodicaly update.
 *   For safety, a watchdog should manage a disconnect timout an set "isDrived" false after a delay.
 * - All input/output modules had to write/read this struct to share data
 */

  // analogic bus data structure
 typedef struct {
  const char* infoName;                         // analog bus short description
  uint16_t value;                               // analog bus current value
  bool isDrived = false;                        // true if the AnalogBus have a driving source
} AnalogBus;

  // digital bus data structure (two state)
 typedef struct {
  const char* infoName;                         // digital bus short description
  bool value;                                   // digital bus current value (true of false)
  bool isDrived = false;                        // true if the DigitalBus have a driving source
} DigitalBus;

  // Main communication bus structure
typedef struct {
  RunLevel runLevel;                            // runlevel state
  AnalogBus* analogBus;                         // analogic bus channels
  DigitalBus* digitalBus;                       // digital bus channels
  uint32_t analogBusMaxVal;                     // Com-bus analog channel maximum value
} ComBus;



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
  const int8_t ID;                           // DC device ID
  const char* infoName;                      // attached device short description
  const DriverPort* drvPort;                 // DC device board driver port
  DcDevType DevType = DcDevType::UNDEFINED;  // attached device type
  DevUsage usage = DevUsage::UNDEFINED;      // attached device usage in the vehicle
  DcDrvMode mode = DcDrvMode::UNDEFINED;     // DC device configuration
  int8_t comChannel = NOT_SET;               // internal com-bus channel used to set the driver speed
  uint32_t pwmFreq = NOT_SET;                // driver PWM frequency (in hz)
  bool polInv = false;                       // driver polarity inversion (true = inverted)
  float maxFwSpeed = PERCENT_MAX;            // maximum forward speed (0 to 100% - Defaut 100%)
  float maxBackSpeed = PERCENT_MAX;          // maximum backward speed (0 to 100% - Defaut 100%)
  const int8_t parentID = NOT_SET;           // parent identifier (used in clone mode)
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





/**
 * @brief Internal input device data structure
 * This structure is used to store input device data.It act as a harware abstraction layer between 
 * devices input module (RC protocol, bluetooth, wifi ...) and internal communication bus(com-bus).
 * Its data structure is devided in channels :
 * - Digital channel to store two state devices (button, switches ...)
 * - Analog channel to store analog devices (sticks, slider, trimmers ...)
 * - A status flag use to track input device state (disconnect timout i.e.)
 * - An analog/digital channels counter
 * 
 * Its instance is create in init\input_init.h file
 * For convenance, a user editable liker map input device data to com-bus channel. By this, it allow
 * 
 *
 * 
 * NOTE:
 * - do not change uint16_t size for devices. Some system sub value depend of this size
 * - "status" flag have to be set true if input device data perodicaly update.
 *   For safety, a input module watchdog should manage a disconnect timout an set "status" to disconnect after a delay.
 * - All input device modules had to write this struct to share incomming data
 */

  // analog devices (sticks, analog buttons, sliders)
typedef struct {
  const char* infoName;                       // device short description
  const int8_t type = NOT_SET;                // analog device type
  uint16_t val;                               // analog device value
  const int32_t minVal = 0;                   // minimum value return by analog device decoder module at lower state
  const int32_t maxVal = 0;                   // maximum value return by analog device decoder module at higher state
  const bool isInverted = false;              // true if analog device axe is inverted
} AnalogDev;

  // digital devices (pushbuttons, switches)
typedef struct {
  const char* infoName;                       // device short description
  const int8_t type = NOT_SET;                // switch device type
  bool val;                                   // digital device value
  const bool isInverted = false;              // true if switch logic is inverted
} DigitalDev;

  // remote data structure
typedef struct {
  int8_t status = NOT_SET;              // remote controle status
  AnalogDev* analogDev;                 // pointer to external AnalogDev structure
  DigitalDev* digitalDev;               // pointer to external DigitalgDev structure
  uint8_t numAnalogCh;                  // number of input analog channel
  uint8_t numDigitalCh;                 // number of input digital channel
} InputDev;

// EOF const.h