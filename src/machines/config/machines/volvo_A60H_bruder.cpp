/*!****************************************************************************
 * @file    volvo_A60H_bruder.cpp
 * @brief   Volvo A60H Bruder — device configuration tables.
 *
 * @details Defines the three device arrays that describe every actuator and
 *   signal source fitted to this vehicle:
 *
 *   - `dcDevArray`  — DC-motor driven devices (traction, steering, dump).
 *   - `SrvDevArray` — Servo-driven devices (none on this build).
 *   - `sigDevArray` — Signal-only sources (horn, lights, key, indicators).
 *
 *   Clone entries (parentID set) inherit their full configuration from the
 *   named parent at init time — only overrides need to be declared.
 *   ComBus channel assignments reference the enums in combus_types.h and
 *   are validated at sound/output module init.
 *******************************************************************************
 */

#include "volvo_A60H_bruder.h"


// =============================================================================
// 1. DC-MOTOR DEVICES
// =============================================================================

/**
 * @brief DC-motor device table for the Volvo A60H Bruder.
 *
 * @details Physical layout:
 *   - 1 steering actuator (or two wired in parallel) on port 1A.
 *   - 6 traction wheel motors: two in the cabin (1B, 2B) and four on the
 *     trailer (3A, 4A, 3B, 4B).  All six share DRIVE_SPEED_BUS and clone
 *     their config from CABIN_LEFT_MOTOR.
 *   - 2 dump actuators wired in parallel on port 2A, sharing DUMP_BUS.
 *     They clone their mode/speed config from STEERING (TWO_WAY_NEUTRAL_CENTER).
 *
 *   polInv corrects wiring direction per port without touching the ComBus value.
 */
DcDevice dcDevArray[DC_DRV_COUNT] = {

  {
    .ID         = STEERING,
    .infoName   = "steering actuators",     ///< one or two actuators wired in //
    .drvPort    = &drvPortArray[DRV_PORT_1A],
    .DevType    = DcDevType::DC_MOTOR,
    .usage      = DevUsage::STEER_MOTOR,
    .mode       = DcDrvMode::TWO_WAY_NEUTRAL_CENTER,
    .comChannel = AnalogComBusID::STEERING_BUS,
    .pwmFreq    = M_DEF_PWM_FREQ,
    .polInv     = true
  },

  {
    .ID         = CABIN_LEFT_MOTOR,
    .infoName   = "cabin left motor",       ///< reference traction device — cloned by all other wheel motors
    .drvPort    = &drvPortArray[DRV_PORT_1B],
    .DevType    = DcDevType::DC_MOTOR,
    .usage      = DevUsage::TRACT_WHEEL,
    .mode       = DcDrvMode::TWO_WAY_NEUTRAL_CENTER,
    .comChannel = AnalogComBusID::DRIVE_SPEED_BUS,
    .pwmFreq    = M_DEF_PWM_FREQ,
    .polInv     = true
  },

  {
    .ID       = CABIN_RIGHT_MOTOR,
    .infoName = "cabin right motor",
    .drvPort  = &drvPortArray[DRV_PORT_2B],
    .polInv   = false,                      ///< opposite wiring to cabin left
    .parentID = CABIN_LEFT_MOTOR
  },

  {
    .ID       = TRAILER_FRONT_LEFT_MOTOR,
    .infoName = "trailer front left motor",
    .drvPort  = &drvPortArray[DRV_PORT_3A],
    .polInv   = false,
    .parentID = CABIN_LEFT_MOTOR
  },

  {
    .ID       = TRAILER_FRONT_RIGHT_MOTOR,
    .infoName = "trailer front right motor",
    .drvPort  = &drvPortArray[DRV_PORT_4A],
    .polInv   = true,
    .parentID = CABIN_LEFT_MOTOR
  },

  {
    .ID       = TRAILER_REAR_LEFT_MOTOR,
    .infoName = "trailer rear left motor",
    .drvPort  = &drvPortArray[DRV_PORT_3B],
    .polInv   = true,
    .parentID = CABIN_LEFT_MOTOR
  },

  {
    .ID       = TRAILER_REAR_RIGHT_MOTOR,
    .infoName = "trailer rear right motor",
    .drvPort  = &drvPortArray[DRV_PORT_4B],
    .polInv   = false,
    .parentID = CABIN_LEFT_MOTOR
  },

  {
    .ID         = DUMP_ACTUATOR,
    .infoName   = "dump actuators L+R",     ///< two actuators wired in parallel
    .drvPort    = &drvPortArray[DRV_PORT_2A],
    .usage      = DevUsage::HYD_LINEAR,
    .comChannel = AnalogComBusID::DUMP_BUS,
    .polInv     = false,
    .parentID   = STEERING                  ///< inherits TWO_WAY_NEUTRAL_CENTER mode
  },
};


// =============================================================================
// 2. SERVO DEVICES
// =============================================================================

/**
 * @brief Servo device table for the Volvo A60H Bruder.
 * @details No servo devices are fitted on this build.
 *   Template entry is left commented out for reference.
 */
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


// =============================================================================
// 3. SIGNAL DEVICES
// =============================================================================

/**
 * @brief Signal device table for the Volvo A60H Bruder.
 *
 * @details Maps each discrete signal source to its ComBus digital channel
 *   and functional role (DevUsage).  Output modules (sound, lighting) iterate
 *   this table at init to build their channel binding tables automatically —
 *   no hard-coded channel indices in those modules.
 *
 *   Entries with DevUsage::UNDEFINED have no standard sound binding; they are
 *   handled by dedicated interpreter logic (engine key FSM, indicator mux…).
 */
SigDevice sigDevArray[SIG_COUNT] = {
  {
    .ID             = HORN_SIG,
    .infoName       = "horn",
    .usage          = DevUsage::SIG_HORN,
    .digitalChannel = DigitalComBusID::HORN
  },
  {
    .ID             = LIGHTS_SIG,
    .infoName       = "lights",
    .usage          = DevUsage::SIG_LIGHT,
    .digitalChannel = DigitalComBusID::LIGHTS
  },
  {
    .ID             = KEY_SIG,
    .infoName       = "ignition key",   ///< interpreted by engine on/off FSM in sound_interpreter
    .usage          = DevUsage::UNDEFINED,
    .digitalChannel = DigitalComBusID::KEY
  },
  {
    .ID             = INDIC_L_SIG,
    .infoName       = "indicator left", ///< encoded into FUNCTION_L mux by sound_core
    .usage          = DevUsage::UNDEFINED,
    .digitalChannel = DigitalComBusID::INDICATOR_LEFT
  },
  {
    .ID             = INDIC_R_SIG,
    .infoName       = "indicator right",
    .usage          = DevUsage::UNDEFINED,
    .digitalChannel = DigitalComBusID::INDICATOR_RIGHT
  },
  {
    .ID          = HAZARDS_SIG,
    .infoName    = "hazards",
    .usage       = DevUsage::UNDEFINED,
    .digitalChannel = DigitalComBusID::HAZARDS
  },
};