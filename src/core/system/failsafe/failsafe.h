/******************************************************************************
 * @file failsafe.h
 * @brief Failsafe module — central orchestration (A16.3 transitional).
 *
 * @details The Failsafe module is **unconditional**: it exists in
 *   every build and does not depend on any `#ifdef`.
 *
 *   At A16.3 the module is in a transitional state:
 *     - `failsafe_init()` is a **no-op** (the per-cycle reset is now
 *       performed by the `cb_reset_fn` processor in the chain itself,
 *       so there is no global pivot to clear at boot).
 *     - `failsafe_update(ComBus& bus)` runs the `kFailsafeChain[]`
 *       via the standard `proc_chain_update()` runner.
 *
 *   This signature is kept stable while the chain is reworked; once
 *   the Failsafe module is fully refactored, `failsafe_init()` will
 *   be removed and `failsafe_update()` will move to its final location.
 *
 *   The Failsafe module is fully **decoupled from RunLevels**: it
 *   does not decide to switch to `IDLE`, `SLEEP`, etc. — it only
 *   publishes the aggregated `DigitalComBusID::FAILSAFE` channel.
 *   Environment-specific reactions (e.g. `main.cpp` reading `FAILSAFE`
 *   to force `RunLevel::SLEEPING`) are handled separately.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>  // ComBus (failsafe_update takes ComBus&)


// =============================================================================
// 1. PUBLIC API (A16.3 transitional)
// =============================================================================

/**
 * @brief Failsafe init — currently a no-op.
 *
 * @details The per-cycle reset is now performed by the `cb_reset_fn`
 *   processor at the head of the FAILSAFE chain, so there is no
 *   global pivot to clear at boot.  Kept for API compatibility —
 *   will be removed when the chain refactor is complete.
 */
void failsafe_init();

/**
 * @brief Failsafe orchestrator — runs `kFailsafeChain[]` via the
 *   standard `proc_chain_update()` runner.
 *
 * @details Delegates to the shared ComBus chain runner, which
 *   implements the full contract:
 *     - seed `value` from `ch.inCh` (none here → 0);
 *     - for each proc: read `proc->inCh` into `proc->inValue`,
 *       call `proc.fn(&proc, value, claimed)`, commit `proc->outValue`
 *       to `proc->outCh` (using `combus_set_digital()` layer-checked);
 *     - commit final `value` to `ch.outCh` (here = `DigitalComBusID::FAILSAFE`).
 *
 *   WIP §6 invariant: every contributor must run every cycle
 *   (the standard runner skips procs when `claimed = true`; Failsafe
 *   procs MUST therefore never claim — see `cb_or.h`).
 *
 *   WIP §6 invariant (timing): every source module update must be
 *   finished **before** `failsafe_update()` is called.  This ordering
 *   is enforced by `sys_manager_update()` and does not depend on the
 *   Failsafe module itself.
 *
 * @param bus  Shared ComBus — forwarded to the standard chain runner.
 */
void failsafe_update(ComBus& bus);

// EOF failsafe.h
