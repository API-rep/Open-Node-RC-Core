/*!****************************************************************************
 * @file    envCfg.cpp
 * @brief   Volvo A60H Bruder — device configuration tables (ESP32_8M_6S board).
 *
 * @details Defines the three device arrays that describe every devices fitted
 *   to this vehicle:
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

#include "envCfg.h"


// =============================================================================
// 1. DC-MOTOR DEVICES
// =============================================================================

/**
 * @brief DC-motor device table for the Volvo A60H Bruder.
 *
 * @details Physical layout:
 *   - 1 steering actuator (or two wired in parallel) on port 1A.
 *   - 6 traction wheel motors: two in the cabin (1B, 2B) and four on the
 *     trailer (3A, 4A, 3B, 4B).  All six share ENGINE_RPM_BUS and clone
 *     their config from CABIN_LEFT_MOTOR.
 *   - 2 dump actuators wired in parallel on port 2A, sharing DUMP_BUS.
 *     They clone their mode/speed config from STEERING (PWM_TWO_WAY_NEUTRAL_CENTER).
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
    .signal     = DcDrvSignal::PWM_TWO_WAY_NEUTRAL_CENTER,
    .comChannel = AnalogComBusID::STEERING_RAMPED_BUS,  ///< reads inertia-smoothed position from sim_ramp
    .pwmFreq    = M_DEF_PWM_FREQ,
    .polInv     = true
  },

  {
    .ID         = CABIN_LEFT_MOTOR,
    .infoName   = "cabin left motor",       ///< reference traction device — cloned by all other wheel motors
    .drvPort    = &drvPortArray[DRV_PORT_1B],
    .DevType    = DcDevType::DC_MOTOR,
    .usage      = DevUsage::TRACT_WHEEL,
    .signal     = DcDrvSignal::PWM_TWO_WAY_NEUTRAL_CENTER,
    .comChannel = AnalogComBusID::ESC_SPEED_BUS,     ///< reads inertia-smoothed speed from sim_ramp (SIM_TRACTION output)
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
    .infoName   = "dump actuators",         ///< two actuators wired in //
    .drvPort    = &drvPortArray[DRV_PORT_2A],
    .comChannel = AnalogComBusID::DUMP_RAMPED_BUS,  ///< reads inertia-smoothed position from sim_ramp
    .parentID   = STEERING
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
//  TEMPLATE     .signal = PWM_TWO_WAY_NEUTRAL_CENTER,
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
// 3. UART PIN TABLE
// =============================================================================

// Values from ESP32_8M_6S.h, included via envCfg.h → boards.h.
const UartPinCfg uartPins[] = {
	{ Txd0Pin,   Rxd0Pin   },  // [0] UART0 — USB / debug serial
	{ -1,        -1        },  // [1] UART1 — unassigned on this board
	{ TxdExtPin, RxdExtPin },  // [2] UART2 — extension port / ComBus TX link
};

const uint8_t uartPinsCount = static_cast<uint8_t>(sizeof(uartPins) / sizeof(uartPins[0]));

// EOF envCfg.cpp
