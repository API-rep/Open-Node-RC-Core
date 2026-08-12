/******************************************************************************
 * @file failsafe_chain.h
 * @brief Failsafe main chain — central CbChain declaration.
 *
 * @details Centralises the declaration of the `kFailsafeChain[]`
 *   table that feeds the `failsafe_update()` orchestrator.
 *
 *   At step 1, the chain is empty: no contributor CbChain is
 *   defined yet. Contributor modules (input link, VBAT, ComBus
 *   link, etc.) will register their own CbChain in the upcoming
 *   roadmap steps.
 *****************************************************************************/
#pragma once

#include <struct/combus_proc_struct.h>  // CbChain


// =============================================================================
// 1. CHAIN REGISTRY
// =============================================================================

/**
 * @brief Failsafe CbChain table — execution order = table order.
 *
 * @details At step 1, the table is empty (`kFailsafeChainCount == 0`):
 *   the pointer is not yet initialised. The table will acquire its
 *   final size at step 2, when the reset processor CbChain is
 *   defined.
 *
 *   The following steps will add, in order:
 *     - the reset processor CbChain (step 2);
 *     - the `proc_failsafe_input_link` CbChain (step 12.5);
 *     - the `proc_failsafe_vbat` CbChain (step 12.6);
 *     - the `proc_failsafe_combus_link` CbChain (step 12.7).
 */
extern CbChain* const kFailsafeChain;  ///< Pointer to the table — nullptr at step 1.

/// @brief Number of CbChain registered in `kFailsafeChain[]`.
///   Equals 0 while the chain is not initialised (step 1).
static constexpr uint8_t kFailsafeChainCount = 0;

// EOF failsafe_chain.h
