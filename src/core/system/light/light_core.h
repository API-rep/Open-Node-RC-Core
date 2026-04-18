/******************************************************************************
 * @file light_core.h
 * @brief Light module — LightCfg parser + neopixel driver.
 *
 * @details Iterates a LightCfg array each loop() pass and drives each
 *   statusLED channel via a type-specific handler (PLAIN_PWM, INDICATOR,
 *   BEACON, REVERSING, TAIL, HIGHBEAM).  Replaces the legacy led() switch-case FSM.
 *
 *   Only compiled when LIGHT_ENABLE is defined.
 *
 * Coupling notes (transitional — addressed at step 10 main.cpp cleanup):
 *   - light_core.cpp includes sound_module/state/sound_state.h for the
 *     soundLeds[] array and statusLED externs.
 *   - Writes gEngineSimState.indicatorOn so the audio task triggers the
 *     indicator click sound (engine_sim_state.h).
 *****************************************************************************/
#pragma once

#ifdef LIGHT_ENABLE

#include "light_state.h"


// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Drive all statusLED channels from the current light state.
 *
 * @details Iterates `descriptors[0..count-1]`.  For each entry, dispatches to
 *   the appropriate handler based on `d.type`:
 *   - PLAIN_PWM   — activeMask / parkMask driven; optional xenon overlay; optional flasherOverride.
 *   - INDICATOR   — flashes on the ComBus indicator channel; forced hazard has priority.
 *   - BEACON      — gyrophare pattern when `LightBit::ROOF` set.
 *   - TAIL        — step-based + full brightness when `LightBit::BRAKING` set.
 *
 * @param state        Current runtime light state (from light_interp_update).
 * @param descriptors  Per-channel config array (constexpr, machine config).
 * @param count        Number of entries in the array.
 * @param mod          Module-level config (cranking flicker, neopixel params…).
 */
void light_core_update(const LightState&      state,
                       const LightCfg*   descriptors,
                       uint8_t                count,
                       const LightModuleCfg&  mod);

/**
 * @brief Drive the FastLED Neopixel bar.
 *
 * @details Rate-limited to one FastLED.show() every 20 ms.
 *   Dispatches on mod.neopixelMode (1–5).  No-op when mod.neopixelCount == 0.
 *
 * @param state  Current runtime light state.
 * @param mod    Module-level config carrying neopixel parameters.
 */
void light_core_update_neopixel(const LightState& state, const LightModuleCfg& mod);

#endif  // LIGHT_ENABLE

// EOF light_core.h
