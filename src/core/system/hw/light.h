/******************************************************************************
 * @file light.h
 * @brief PWM-driven light and shaker channel init — board config driven.
 *
 * @details Manages a board-configured array of PWM output channels
 *   (LEDs, shaker motor) using the statusLED abstraction layer.
 *   Configuration is provided via `LightPort` (defined in machines_struct.h),
 *   following the same ptr+count container pattern used by `SwitchPort`,
 *   `VBatSense`, and `Board`.
 *
 *   The `statusLED` object array is provided by the caller so that this
 *   module stays independent of any environment's global state layout.
 *
 *   Registration : call `light_init()` once with the board LightPort and the
 *                  matching array of statusLED pointers.
 *   Read         : `light_count()` returns the total number of configured
 *                  channels (active and disabled).
 *
 * @note `light_init()` is only declared when `<statusLED.h>` is available in
 *   the build environment.  Environments without statusLED (e.g. machine
 *   node) compile this header without the init function — zero overhead.
 *****************************************************************************/
#pragma once

#include <struct/machines_struct.h>

/// LEDC PWM frequency for all light and shaker channels (Hz).
static constexpr uint32_t LightPwmFreq = 20000u;

// =============================================================================
// 1. API
// =============================================================================

#if __has_include(<statusLED.h>)
#  include <statusLED.h>
#  include "pin_reg.h"

/// Initialise all PWM-driven light and shaker channels.
/// @param port  Board LightPort — config array + channel count (must outlive all calls).
/// @param leds  Caller-owned array of `statusLED*`, same count and order as `port->cfg[]`.
/// Channels whose pin is already owned by another peripheral in `reg` are skipped.

void light_init(LightPort* port, statusLED** leds, PinReg& reg);

#endif  // __has_include(<statusLED.h>)

/// Return the number of registered light channels (0 before `light_init`).
uint8_t light_count();

// EOF light.h
