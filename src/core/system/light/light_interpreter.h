/******************************************************************************
 * @file light_interpreter.h
 * @brief Light module — ComBus digital snapshot → LightState translator.
 *
 * @details Translates one ComBus frame into a LightState each loop pass.
 *   All persistent edge-detect state is file-scoped inside the .cpp — the
 *   caller has no lifecycle to manage beyond calling light_interp_update().
 *
 *   Only compiled when LIGHT_ENABLE is defined.
 *****************************************************************************/
#pragma once

#ifdef LIGHT_ENABLE

#include <struct/combus_struct.h>
#include "light_state.h"


// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Translate the current ComBus snapshot into a LightState.
 *
 * @details Builds runtimeMask in 3 layers:
 *   - Layer 1 : lightsState FSM — 6-step cycle on LIGHTS rising edge (stored in LightState, not in mask).
 *   - Layer 2 : RunLevel table — mod.runLevelMask[bus.runLevel] OR'd into mask.
 *   - Layer 3 : ComBus channel parsing — all remaining bits derived from bus values:
 *       ESC_SPEED_BUS < centre      → REVERSING (when engineRunning)
 *       DigitalComBusID::BRAKING    → BRAKING  (MotionOutput::isBraking)
 *       DigitalComBusID::LOW_BEAM   → LOW_BEAM_ON
 *       DigitalComBusID::HIGH_BEAM  + LOW_BEAM_ON set → HIGH_BEAM
 *       DigitalComBusID::INDICATOR_LEFT / HAZARDS / batteryLow → IND_L / IND_R
 *       DigitalComBusID::INDICATOR_RIGHT / HAZARDS / batteryLow → IND_R
 *       DigitalComBusID::ROOF_LIGHT → ROOF
 *       (flasher: TBD — no dedicated channel yet)
 *
 * @note FSM step-skip: steps 1 (cab) and 4 (fog) are skipped automatically
 *   when mod.skipCabStep / mod.skipFogStep are set, preventing empty button presses
 *   when those light circuits are not wired.
 *
 * @param bus    Live ComBus instance — read-only.
 * @param mod    Module-level config — skipCabStep / skipFogStep used for FSM skip logic.
 * @param state  LightState to update — written in place.
 */
void light_interp_update(const ComBus& bus, const LightModuleCfg& mod, LightState& state);

#endif  // LIGHT_ENABLE

// EOF light_interpreter.h
