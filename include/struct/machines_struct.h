/*!****************************************************************************
 * @file  machines_struct.h
 * @brief machines structure and variable definition file
 * This file contain all structure used to define machine configuration and features.
 *******************************************************************************/// 
#pragma once

#include <stdint.h>
#include <optional>

#include <pin_defs.h>

#include <const.h>
#include <defs/defs.h>

// Provides the correct namespace-scoped enum declarations + using namespace for
// AnalogComBusID/DigitalComBusID used below. This fix project dependencies that 
// could create include cycles.
#include <core/config/combus/combus_ids.h>


/**
 * @brief DC driver module structure
 * Contain configuration of DC driver module feature
 */

typedef struct {
  const char* infoName;
  const ActiveLevel sleepActiveLevel;   // driver sleep mode (Unset by default)
  const ActiveLevel enableActiveLevel;  // driver enable mode (Unset by default)
  const DecayMode   DecayPinHighState;  // decay mode when decay pin is high (Unset by default)
  const DecayMode   defaultdDecayMode;  // driver decay mode (Unset by default)
  const PinMode     faultMode;          // driver fault mode (unset by default)
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
  std::optional<uint8_t> dirPin;
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
 * @brief Board ESC output port structure.
 *
 * @details Combines protocol type and GPIO assignments for one ESC channel.
 *   hw_init_esc selects ServoCore (PWM_SERVO_SIG) or DcMotorCore (PWM_HBRIDGE)
 *   at init time based on escType — callers use esc_write() without any
 *   protocol-specific branching.
 *
 *   dirPin is only required for EscType::PWM_HBRIDGE bidirectional mode.
 */
typedef struct {
  const int8_t           ID;
  const char*            infoName;
  EscType                escType = EscType::UNDEFINED;  ///< Protocol / library selector
  std::optional<uint8_t> pwmPin;                        ///< PWM output GPIO
  std::optional<uint8_t> dirPin;                        ///< Direction pin (PWM_HBRIDGE only)
} EscPort;


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
  std::optional<AnalogComBusID> comChannel;   // internal com-bus channel used to set the driver speed
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
  const int8_t ID;                           // servo ID
  const char* infoName;                      // device short description
  const ServoPort* srvPort;                  // DC device board driver port
  SrvDevType type = SrvDevType::UNDEFINED;   // device wired to servo output
  std::optional<uint8_t> usage;              // device usage in the vehicle
  std::optional<uint8_t> mode;               // servo configuration
  std::optional<AnalogComBusID> comChannel;  // internal com-bus used to set the servo angle
  std::optional<uint32_t> pwmFreq;           // output PWM frequency (in hz)
  bool isInverted = false;                   // servo sense inversion (true = inverted)
  std::optional<float> minAngleLimit;        // servo minimun angle limit (0 to min HW angle)
  std::optional<float> maxAngleLimit;        // servo maximum angle limit (0 to max HW angle)
  std::optional<float> zeroAtHwAngle;        // servo zero at hardware angle (min HW angle to max HW angle, typ. 0)
  std::optional<float> maxSpeed;             // servo maximum speed
  std::optional<float> maxAccel;             // servo maximum acceleration
  const std::optional<uint8_t> parentID;     // parent identifier (used in clone mode)
} SrvDevice;


/**
 * @brief Signal device structure
 *
 * @details Represents a discrete or continuous signal source (horn, lights,
 *   indicator, key…) that has no physical driver port and is read directly
 *   from a ComBus channel.  Output modules (sound, lighting…) use
 *   `usage` to determine the correct action for each signal.
 *
 *   Exactly one of `analogChannel` / `digitalChannel` must be set.
 *   The init code validates this and logs an error for misconfigured entries.
 */
typedef struct {
  const int8_t  ID;                                     // device ID (enum from machine header)
  const char*   infoName;                               // short description
  DevUsage      usage = DevUsage::UNDEFINED;            // signal role (SIG_HORN, SIG_LIGHT…)
  std::optional<AnalogComBusID>  analogChannel;         // set if signal source is a continuous channel
  std::optional<DigitalComBusID> digitalChannel;        // set if signal source is a discrete channel
} SigDevice;


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
  DcDevice*  dcDev;                                      // DC driver device config structure
  uint8_t    dcDevCount;                                  // number of DC driver device configured
  SrvDevice* srvDev;                                      // servo device config structure
  uint8_t    srvDevCount;                                 // number of servo device configured
  SigDevice* sigDev     = nullptr;                        // signal device config structure (optional)
  uint8_t    sigDevCount = 0;                             // number of signal devices configured
} Machine;


// =============================================================================
// SWITCH PORT SENSING
// =============================================================================

/**
 * @brief Hardware configuration for one polling-based switch input.
 *
 * @details Defined once per physical switch in the board config file.
 *   The switch manager reads this at init time to configure the GPIO and
 *   debounce parameters.  All fields are const — they must not change at runtime.
 */
struct SwitchPortCfg {
  const char* infoName;    ///< Human-readable channel name.
  int8_t      pin;         ///< GPIO pin number (-1 = disabled / slot unused).
  bool        pullUp;      ///< true = INPUT_PULLUP, false = INPUT_PULLDOWN.
  uint16_t    debounceMs;  ///< Debounce window in ms (0 = immediate).
};

/**
 * @brief Runtime state for one switch channel.
 *
 * @details Written exclusively by the switch manager — treat as read-only from
 *   callers.  Zero-initialised by default construction.
 */
struct SwitchPortState {
  bool     confirmed;   ///< Last debounced digitalRead() level.
  bool     pending;     ///< Candidate level pending debounce confirmation.
  uint32_t pendingMs;   ///< millis() timestamp of the most-recent raw level change.
};

/**
 * @brief Top-level switch port container.
 *
 * @details cfg points to the board-defined config array (flash).
 *   state points to the module runtime state array (RAM).
 *   count holds the number of active channels.
 *   Mirrors the VBatSense / Board / Machine container pattern.
 */
struct SwitchPort {
  const SwitchPortCfg* cfg   = nullptr;  ///< Pointer to board config array (flash).
  uint8_t              count = 0;        ///< Number of registered channels.
  SwitchPortState*     state = nullptr;  ///< Pointer to runtime state array (RAM).
};


// =============================================================================
// LIGHT PORT CONFIG
// =============================================================================

/**
 * @brief Hardware configuration for one PWM-driven light channel.
 *
 * @details Defined once per channel in the board config file.
 *   `light_init()` iterates these entries, requests a PwmControl lease from
 *   PwmBroker for each active pin, and calls `statusLED::begin()`.  Pins set
 *   to -1 (or implicitly cast from `SOUND_NO_LED_PIN`) are skipped silently.
 *   LEDC channel allocation is handled entirely by PwmBroker — no ledcCh
 *   field needed here.
 */

struct LightCfg {
  const char* infoName;   ///< Human-readable channel name.
  int8_t      pin;        ///< GPIO pin (-1 = disabled, e.g. SOUND_NO_LED_PIN cast to int8_t).
};



/**
 * @brief Board light container — passed to light_init().
 *
 * @details cfg points to the board config array (flash).
 *   count holds the total number of channels (active and disabled).
 *   Runtime brightness and pattern state live inside each statusLED instance.
 */

struct LightPort {
  const LightCfg* cfg;    ///< Pointer to board config array (flash).
  uint8_t         count;  ///< Number of registered channels.
};

// EOF machines_struct.h