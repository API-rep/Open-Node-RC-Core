/*!****************************************************************************
 * @file  ctrl_struct.h
 * @brief Control channel pipeline â€” aliases and behaviour configs.
 *
 * @details The ctrl layer shares `CbProc`/`CbChannel`/`CbProcFn` with the
 *   simulation layer (defined in `cb_struct.h`).
 *
 *   Architecture (mirrors SimChannel pipeline):
 *   @code
 *     CbChannel.optInCh  (runner pre-reads â€” raw button)
 *       â†’ CtrlProc[0..n-1]  (behaviour functions â€” speed gate, toggle, â€¦)
 *     CbChannel.optOutCh (runner post-writes â€” processed state)
 *   @endcode
 *
 *   Pipeline contract (from cb_struct.h):
 *   - `value` carries the channel state as uint16_t (0 = false, nonzero = true).
 *   - `claimed` may be set true by any proc to abort the remaining chain.
 *     The runner ALWAYS writes `optOutCh` after the chain, even when claimed.
 *   - `cfg`    â€” `const void*` pointer to a constexpr config in flash.
 *   - `state`  â€” `void*` pointer to mutable runtime state in RAM.
 *   - `secInCh[i]` / `secInValue[i]` â€” secondary bus inputs injected by runner.
 *
 *   **Adding a new ctrl proc:**
 *   1. Add `MyCtrlCfg` + `MyCtrlState` in this file.
 *   2. Create `ctrl_<name>.h/.cpp` in `src/core/system/simulation/`.
 *   3. Register in `ctrl_config.cpp` of the relevant vehicle folder.
 *   4. Nothing else changes here.
 *****************************************************************************/
#pragma once

#include <struct/cb_struct.h>  // CbProc, CbChannel, CbProcFn


// =============================================================================
// 1. TYPE ALIASES
// =============================================================================

using CtrlProcFn  = CbProcFn;   ///< Control proc function pointer (= CbProcFn, no bus).
using CtrlProc    = CbProc;     ///< Control proc descriptor (= CbProc).
using CtrlChannel = CbChannel;  ///< Control channel descriptor (= CbChannel).


// =============================================================================
// 2. SPEED GATE PROC CONFIG  (ctrl_speed_gate_fn â€” see ctrl_speed_gate.h)
// =============================================================================

/**
 * @brief Config for `ctrl_speed_gate_fn` â€” suppresses engagement above a speed threshold.
 *
 * @details Gate logic:
 *   - If `maxEngageSpd == 0`: gate is a no-op (always allow).
 *   - If `secInValue[1]` (active channel) is nonzero: gate bypasses â€”
 *     disengagement is always allowed regardless of speed.
 *   - Otherwise: suppresses `value` when `secInValue[0]` (speed) > `maxEngageSpd`.
 *
 *   Bus channel IDs are declared in the proc's `secInCh[]`:
 *   - `secInCh[0]` â€” analog speed channel (e.g. RPM_BUS).
 *   - `secInCh[1]` â€” digital active-state channel (e.g. DIRECT_DRIVE).
 *
 *   Placement: insert before `ctrl_toggle_fn` in the proc chain.
 */
struct CtrlSpeedGateCfg {
    uint16_t  maxEngageSpd;  ///< Max engage speed (ComBus units). 0 = no guard.
};


// =============================================================================
// 3. TOGGLE PROC STATE  (ctrl_toggle_fn â€” see ctrl_toggle.h)
// =============================================================================

/**
 * @brief Mutable runtime state for `ctrl_toggle_fn`.
 *
 * @details `ctrl_toggle_fn` has no config (pass `nullptr` for `cfg`).
 *   Zero-initialised by default construction.
 *   `active` is stored as uint16_t (0 = inactive, 1 = active) to match the
 *   `uint16_t& value` pipeline convention.
 */
struct CtrlToggleState {
    uint16_t active  = 0u;  ///< Current output state (0 = OFF, 1 = ON).
    uint16_t prevBtn = 0u;  ///< Raw button value last cycle â€” rising-edge detection.
};

// EOF ctrl_struct.h
