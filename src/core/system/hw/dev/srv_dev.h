/******************************************************************************
 * @file srv_dev.h
 * @brief Servo device configuration check and initialization.
 *
 * @details Provides the generic entry points for servo outputs driven by a
 *   SrvDevice descriptor array contained in an EnvCfg.
 *   srvDevObj is the dynamically allocated ServoCore array; it is allocated
 *   by servoInit() and exposed extern for modules that need direct servo
 *   control (e.g. light_servo_beacon).
 *
 *   servoInit() integrates the configuration check as its first step — the
 *   caller does not need to call checkSrvHwConfig() separately.
 ******************************************************************************/
#pragma once

#include <ServoCore.h>
#include <core/config/machines/combus_types.h>
#include <core/system/hw/pin_reg.h>


// =============================================================================
// 1. SERVO INSTANCE ARRAY  (defined in srv.cpp)
// =============================================================================

/// Dynamic ServoCore array allocated by servoInit().  Nullptr until called.
/// Indexed by vehicle servo ID (SrvDevice::ID).
extern ServoCore* srvDevObj;


// =============================================================================
// 2. CONFIGURATION CHECK
// =============================================================================

/**
 * @brief Verify servo device configuration coherence.
 *
 * @details Checks that each servo array index matches its declared ID, that
 *   the hwAngle range is valid, and that the PWM tick window is rational.
 *   Returns true when at least one error is detected — halting is the
 *   caller's responsibility.
 */
bool checkSrvHwConfig(const EnvCfg& config);


// =============================================================================
// 3. INITIALIZATION
// =============================================================================

/**
 * @brief Allocate and configure all servo outputs from a vehicle descriptor.
 *
 * @details Calls checkSrvHwConfig() as first step; aborts on config error.
 *   Allocates srvDevObj[], claims each board pin via reg, applies the
 *   descriptor (PWM frequency, tick endpoints, hardware angle range), and
 *   parks each servo at centre (0.0f).
 *   No-op when config.srvDev is nullptr or config.srvDevCount is 0.
 *
 * @param config  Vehicle EnvCfg containing the srvDev array and srvDevCount.
 * @param reg     Board pin registry for conflict detection.
 */
void servoInit(const EnvCfg& config, PinReg& reg);

// EOF srv.h
