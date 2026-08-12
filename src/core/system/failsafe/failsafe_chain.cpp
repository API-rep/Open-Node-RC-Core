/******************************************************************************
 * @file failsafe_chain.cpp
 * @brief Failsafe main chain — registry definition.
 *
 * @details At step 1, the `kFailsafeChain` registry is a null pointer
 *   and `kFailsafeChainCount` equals 0: no CbChain is registered
 *   yet. The `failsafe_update()` orchestrator detects this case and
 *   iterates nothing.
 *
 *   In the upcoming steps, the pointer will be re-initialised to
 *   point to a static table containing the contributor CbChains.
 *****************************************************************************/

#include "failsafe_chain.h"


// =============================================================================
// 1. CHAIN REGISTRY — DEFINITION
// =============================================================================

/// @brief Registry pointer definition — nullptr while the chain is empty.
CbChain* const kFailsafeChain = nullptr;

// EOF failsafe_chain.cpp
