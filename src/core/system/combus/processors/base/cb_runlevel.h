/******************************************************************************
 * @file  cb_runlevel.h
 * @brief CbProc — RunLevel setters (continuous + edge-triggered variants).
 *
 * @details Two processor functions are provided, both sharing the same
 *   `CbRunlevelCfg` (high/low/claim) but differing in WHEN they write:
 *
 *   - cb_runlevel_fn (continuous / stateless):
 *       Writes `value` to `cfg->high` or `cfg->low` on EVERY cycle, depending
 *       on the lever state.  No state required.  Use when the lever must
 *       keep the channel pinned to a value as long as the condition holds
 *       (e.g. failsafe: while FAILSAFE=true, force RUNLEVEL=IDLE).
 *
 *   - cb_runlevel_once_fn (edge-triggered / stateful):
 *       Writes `value` ONLY on a transition of the lever (inValue != prevValue).
 *       Requires a `CbRunlevelOnceState` per instance.  Use when the lever
 *       drives a one-shot transition (e.g. KEY_ACTIVE 0→1 → STARTING, then
 *       leave RUNLEVEL alone so the FSM in main.cpp can progress to RUNNING).
 *
 *   Both variants manipulate the pipeline `value` directly (read-modify-write
 *   pattern), NOT via a side-effect call to `combus_set_analog()`.
 *
 *   Behaviour (shared by both variants, applied only when the variant decides
 *   to act):
 *     - lever == true  AND cfg->high.has_value()  → value = cfg->high.value()
 *     - lever == false AND cfg->low.has_value()   → value = cfg->low.value()
 *     - if cfg->claim == true AND a write occurred → claimed = true
 *     - if the corresponding branch is std::nullopt → no write, no claim
 *
 *   Typical usage in a dedicated runlevel chain (inCh = RUNLEVEL, outCh = RUNLEVEL):
 *   @code
 *     // failsafe instance — continuous: forces IDLE while FAILSAFE=true
 *     { .inCh = FAILSAFE, .fn = cb_runlevel_fn,
 *       .cfg = { .high = RunLevel::FAILSAFE, .low = std::nullopt, .claim = true } }
 *     // runlevel instance — edge-triggered: KEY_ACTIVE 0→1 → STARTING (once)
 *     { .inCh = KEY_ACTIVE, .fn = cb_runlevel_once_fn,
 *       .cfg = { .high = RunLevel::STARTING, .low = RunLevel::TURNING_OFF, .claim = false } }
 *   @endcode
 ******************************************************************************/
#pragma once

#include <optional>
#include <defs/machines_defs.h>         // RunLevel
#include <struct/combus_proc_struct.h>   // CbProc


// =============================================================================
// 1. CONFIG
// =============================================================================

/**
 * @brief Static configuration for the RunLevel setter processors.
 *
 * @details `high` and `low` are independent — either may be std::nullopt to
 *   disable that branch.  When the lever selects a branch whose value is
 *   nullopt, the proc performs no write and does not claim the chain.
 */
struct CbRunlevelCfg {
    std::optional<RunLevel> high;   ///< Value to write when lever == true.  nullopt = no write.
    std::optional<RunLevel> low;    ///< Value to write when lever == false. nullopt = no write.
    bool                     claim; ///< Claim the chain when a write actually occurred.
};


// =============================================================================
// 2. STATE (edge-triggered variant only)
// =============================================================================

/**
 * @brief Mutable runtime state for the edge-triggered RunLevel setter.
 *
 * @details Zero-init is valid — `prevValue = 0` means "no prior lever state".
 *   Assign one instance per CbProc that uses `cb_runlevel_once_fn`.
 */
struct CbRunlevelOnceState {
    uint16_t prevValue = 0u;   ///< Previous lever reading (0 or non-zero).
};


// =============================================================================
// 3. PUBLIC API
// =============================================================================

/**
 * @brief Continuous (stateless) RunLevel setter processor function.
 *
 * @details Reads the lever state from `proc->inValue` (injected by the runner
 *   from the configured `inCh`).  Writes `value` to `cfg->high` or `cfg->low`
 *   on EVERY cycle depending on the lever.  Sets `claimed = true` when
 *   `cfg->claim` is true AND a write actually occurred.
 *
 * @param proc     CbProc descriptor.  `cfg` = CbRunlevelCfg*.  No state used.
 * @param value    Pipeline value (in/out) — modified when the selected branch is defined.
 * @param claimed  Set to `true` when `cfg->claim` is true and a write occurred.
 */
void cb_runlevel_fn(CbProc* proc, uint16_t& value, bool& claimed);


/**
 * @brief Edge-triggered (stateful) RunLevel setter processor function.
 *
 * @details Reads the lever state from `proc->inValue`.  Compares it to
 *   `state->prevValue`:
 *     - No transition (inValue == prevValue) → no write, no claim, exit.
 *     - Transition detected (inValue != prevValue) → apply the same logic
 *       as `cb_runlevel_fn` (write cfg->high or cfg->low, claim if requested).
 *   In all cases, `state->prevValue` is updated to `proc->inValue` on exit.
 *
 * @param proc     CbProc descriptor.  `cfg` = CbRunlevelCfg*, `state` = CbRunlevelOnceState*.
 * @param value    Pipeline value (in/out) — modified only on transition.
 * @param claimed  Set to `true` when `cfg->claim` is true and a write occurred.
 */
void cb_runlevel_once_fn(CbProc* proc, uint16_t& value, bool& claimed);


// EOF cb_runlevel.h