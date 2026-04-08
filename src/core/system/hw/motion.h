/******************************************************************************
 * @file motion.h
 * @brief ESC + servo motion-control — config validation and per-cycle update.
 *
 * @details Two operating modes, selected by pointer pattern in MotionConfig:
 *   - Simple ramp  (ramp != null)          : hydraulics, steering.
 *   - Traction     (gear + inertia != null): wheel / track drive.
 * 
 *  Algorithm:
 *   - ramp : single ramp period + accel/brake step sizes; same timing in both
 *       directions, no gear or inertia model.
 *   - gear : per-gear ramp periods (1st / 2nd / 3rd / crawler) + global accel
 *       scaling; determines how fast the speed ramps up.
 *   - inertia : step sizes for accel and brake; brakeMargin transmitted to the
 *       sound engine to signal engine braking.
 *
 *   `motion_check()`  — validate a config at init time.
 *   `motion_update()` — advance the ramp one cycle, write ComBus.
 *
 *   Both are no-ops without -D MOTION_ENABLED.
 *****************************************************************************/
#pragma once

#include <struct/motion_struct.h>   // MotionConfig, MotionRuntime, MotionOutput


// =============================================================================
// 1. CONFIG VALIDATION
// =============================================================================

/**
 * @brief Validate a `MotionConfig` at init time.
 *
 * @details Algorithm selection by pointer pattern (ramp XOR gear+inertia):
 *
 *   Mode         | ramp  | gear  | inertia
 *   -------------|-------|-------|--------
 *   Simple ramp  |  set  |  null |  null
 *   Traction     |  null |  set  |  set
 *
 *   Value checks: hw->maxHwVal > minHwVal,  band within effective limits,
 *                 margin (optional) within hw,  band->max >= band->min.
 *
 * @param cfg  Pointer to the config to validate.  Must not be null.
 * @return     True when config is valid.  False + fatal log on first violation.
 */

bool motion_check(const MotionConfig* cfg);



// =============================================================================
// 2. PIPELINE UPDATE
// =============================================================================

/**
 * @brief Motion pipeline entry point. Call each control cycle to process rawComBusVal.
 *
 * @details The ramp algorithm is selected automatically from the config pointer pattern
 *   (`config->ramp` for simple ramp, `config->gear` for traction) — see `motion_check()` for
 *   the valid combinations.  When `config->comBus` is set and `owner == SYSTEM`, the
 *   filtered value is written to the ComBus channel automatically.
 *
 * @param rawComBusVal  Desired ComBus value, unfiltered (straight from the receiver).
 * @param config        Ramp/traction config — must have passed `motion_check()` at init.
 * @param runtime       Per-device runtime state, updated in place every call.
 * @param output        Optional output struct, used by sub-modules (e.g. sound engine) to read
 *                      computed state (isBraking, currentSpeed…).  Pass `nullptr` if not used.
 *
 * @note ComBus write is the caller's responsibility.  After this call, write
 *   `runtime->currentPos` to the ComBus channel identified by
 *   `DcDevice::comChannel`.
 */

void motion_update(combus_t             rawComBusVal,
                   const MotionConfig*  config,
                   MotionRuntime*       runtime,
                   MotionOutput*        output);


// EOF motion.h
