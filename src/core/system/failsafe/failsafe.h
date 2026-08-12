/******************************************************************************
 * @file failsafe.h
 * @brief Central Failsafe module — pivot and orchestration.
 *
 * @details Central Failsafe module. It is **unconditional**: it exists
 *   in every build and does not depend on any `#ifdef`.
 *
 *   At step 1 (roadmap WIP §12.1), the module exposes:
 *     - a global `FailsafeBus` pivot (`failsafeBus`) — `active: bool`;
 *     - an orchestrator `failsafe_update()`;
 *     - an initialiser `failsafe_init()`;
 *     - a read-only façade `failsafe_is_active()` (separate header).
 *
 *   The processor chain (`kFailsafeChain`) is declared but empty at
 *   step 1: `failsafe_update()` is therefore deliberately **no-op**.
 *   The pivot reset will be performed exclusively by the processor
 *   `proc_failsafe_reset` at step 2 — nothing is done inline here.
 *
 *   The Failsafe module is fully **decoupled from RunLevels**: it
 *   does not decide to switch to `IDLE`, `SLEEP`, etc. Environment-
 *   specific reactions are handled separately under
 *   `env/config/failsafe/`.
 *****************************************************************************/
#pragma once

#include <stdint.h>  // uint8_t (chain count, future-proof)


// =============================================================================
// 1. PIVOT
// =============================================================================

/**
 * @brief Global Failsafe pivot — current cycle state.
 *
 * @details At step 1, only the `active` field is defined. Additional
 *   fields (counters, `FAILSAFE_X` sub-ComBuses, etc.) will be added
 *   in the next steps, exclusively **inside this pivot**.
 *
 *   Writing to `active` is strictly internal to the Failsafe module
 *   (reset processor at step 2, contributor processors at steps
 *   12.5+). No public API allows external modules to modify it.
 */
struct FailsafeBus
{
    bool active;  ///< true if at least one fault was detected during the current cycle.
};

/**
 * @brief Global instance of the Failsafe pivot.
 *
 * @details Defined in `failsafe.cpp`, zero-initialised by default
 *   (`active = false`). The pivot is shared by every module that
 *   consults the Failsafe state through `failsafe_is_active()`.
 */
extern FailsafeBus failsafeBus;


// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Failsafe initialisation — called once at boot.
 *
 * @details At step 1, initialisation is limited to clearing the pivot
 *   (`failsafeBus.active = false`). No ComBus is stored internally:
 *   the Failsafe module does not keep any reference to a main ComBus;
 *   the `FAILSAFE_X` sub-ComBuses will be managed in the next steps
 *   by their respective owners.
 *
 *   Call site planned in `sys_init.cpp` at step 12.1.4.
 */
void failsafe_init();

/**
 * @brief Failsafe orchestrator — called every cycle by `sys_manager`.
 *
 * @details At step 1, **deliberate no-op**: the chain
 *   `kFailsafeChain` is empty (`kFailsafeChainCount == 0` and
 *   `kFailsafeChain == nullptr`), so no iteration is attempted. No
 *   inline reset is executed; the pivot is left untouched.
 *
 *   At step 2, this function will iterate the main chain, whose
 *   first entry will be `proc_failsafe_reset` (which will clear the
 *   pivot) followed by the contributor CbChains (steps 12.5+).
 *
 *   WIP §6 invariant: every source module update must be finished
 *   **before** `failsafe_update()` is called. This ordering is
 *   enforced by `sys_manager_update` and does not depend on the
 *   Failsafe module itself.
 */
void failsafe_update();

// EOF failsafe.h
