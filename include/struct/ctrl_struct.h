/*!****************************************************************************
 * @file  ctrl_struct.h
 * @brief Control channel pipeline — aliases and behaviour configs.
 *
 * @details The ctrl layer shares `CbProc`/`CbChain`/`CbProcFn` with the
 *   simulation layer (defined in `cb_struct.h`).
 *
 *   Architecture (mirrors CbChain pipeline):
 *   @code
 *     CbChain.optInCh  (runner pre-reads — raw button)
 *       → CbProc[0..n-1]  (behaviour functions — speed gate, toggle, …)
 *     CbChain.outCh (runner post-writes — processed state)
 *   @endcode
 *
 *   Pipeline contract (from cb_struct.h):
 *   - `value` carries the channel state as uint16_t (0 = false, nonzero = true).
 *   - `claimed` may be set true by any proc to abort the remaining chain.
 *     The runner ALWAYS writes `outCh` after the chain, even when claimed.
 *   - `cfg`    — `const void*` pointer to a constexpr config in flash.
 *   - `state`  — `void*` pointer to mutable runtime state in RAM.
 *   - `inCh[i]` / `inValue[i]` — secondary bus inputs injected by runner.
 *
 *   **Adding a new ctrl proc:**
 *   1. Add `MyCtrlCfg` + `MyCtrlState` in this file.
 *   2. Create `ctrl_<name>.h/.cpp` in `src/core/system/simulation/`.
 *   3. Register in `ctrl_config.cpp` of the relevant vehicle folder.
 *   4. Nothing else changes here.
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>  // CbProc, CbChain, CbProcFn



// =============================================================================
// 1. TOGGLE PROC STATE  (ctrl_toggle_fn — see ctrl_toggle.h)
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
    uint16_t prevBtn = 0u;  ///< Raw button value last cycle — rising-edge detection.
};

// EOF ctrl_struct.h
