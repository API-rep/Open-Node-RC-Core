/******************************************************************************
 * @file drv_dev.h
 * @brief DC driver configuration check and initialization.
 *
 * @details Provides the generic entry points for DC motor outputs driven by a
 *   DcDevice descriptor array contained in an EnvCfg.
 *   dcDevObj is the dynamically allocated DcMotorCore array; allocated by
 *   dcDriverInit() and exposed extern for modules that need direct motor
 *   control.
 *
 *   dcDriverInit() integrates clone propagation (applyParentConfig), the
 *   configuration check, array allocation, and hardware setup as sequential
 *   steps — the caller does not need to call these separately.
 ******************************************************************************/
#pragma once

#include <DcMotorCore.h>
#include <core/config/machines/combus_types.h>
#include <core/system/hw/pin_reg.h>


// =============================================================================
// 1. DC DRIVER INSTANCE ARRAY  (defined in drv.cpp)
// =============================================================================

/// Dynamic DcMotorCore array allocated by dcDriverInit().  Nullptr until called.
/// Indexed by vehicle DC driver ID (DcDevice::ID).
extern DcMotorCore* dcDevObj;


// =============================================================================
// 2. CONFIGURATION CHECK
// =============================================================================

/**
 * @brief Verify DC driver configuration coherence.
 *
 * @details Checks that each driver array index matches its declared ID and
 *   that every declared motion config passes motion_check().
 *   Returns true when at least one error is detected — halting is the
 *   caller's responsibility.
 */
bool checkDrvHwConfig(const EnvCfg& config);


// =============================================================================
// 3. INITIALIZATION
// =============================================================================

/**
 * @brief Allocate and configure all DC motor outputs from a vehicle descriptor.
 *
 * @details Propagates parent fields to clone entries, calls checkDrvHwConfig(),
 *   allocates dcDevObj[], then configures each driver: PWM frequency, pin claim
 *   (DIR/FLT/SLP/EN/BRK), and attaches the PWM channel (clone or master timer).
 *   No-op when config.dcDev is nullptr or config.dcDevCount is 0.
 *
 * @param config  Vehicle EnvCfg containing the dcDev array and dcDevCount.
 * @param reg     Board pin registry for conflict detection.
 */
void dcDriverInit(const EnvCfg& config, PinReg& reg);

// EOF drv_dev.h
