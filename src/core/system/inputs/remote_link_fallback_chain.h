/******************************************************************************
 * @file remote_link_fallback_chain.h
 * @brief REMOTE_LINK_LOST → IDLE fallback chain — registry declarations.
 *
 * @details Companion header to remote_link_fallback_chain.cpp.  Exposes the
 *   static chain table and count for the runner to iterate over.  When
 *   HAS_REMOTE_LINK_LOST_FALLBACK is NOT defined, the chain table is a
 *   placeholder and the count is 0 — callers can always iterate without
 *   a separate `#if` guard.
 *
 *   The chain is OPTIONAL: only emitted when the project defines
 *   `-D HAS_REMOTE_LINK_LOST_FALLBACK` (autonomous machines do not need it).
 *****************************************************************************/
#pragma once

#include <cstdint>
#include <struct/combus_proc_struct.h>   // CbChain


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Static chain table (single chain: remote_link_fallback_main).
 *
 * @details Defined in remote_link_fallback_chain.cpp.  When the fallback is
 *   disabled (HAS_REMOTE_LINK_LOST_FALLBACK not defined), this table is a
 *   placeholder of size 1 with an empty entry; kRemoteLinkFallbackChainCount
 *   is 0 so the runner's loop is a no-op.
 */
extern CbChain kRemoteLinkFallbackChain[];

/**
 * @brief Static chain count — equals the array length.
 *
 * @details 0 when HAS_REMOTE_LINK_LOST_FALLBACK is not defined.
 */
extern const uint8_t kRemoteLinkFallbackChainCount;


// =============================================================================
// 2. CONVENIENCE — single-chain façade
// =============================================================================

/**
 * @brief Run the REMOTE_LINK_LOST → IDLE fallback chain once.
 *
 * @details Convenience façade equivalent to
 *   `proc_chain_update(kRemoteLinkFallbackChain, kRemoteLinkFallbackChainCount, bus)`.
 *   No-op when the fallback is disabled (count == 0).
 *
 * @param bus  Live ComBus instance (passed to the chain runner).
 */
void remote_link_fallback_update(ComBus& bus);


// EOF remote_link_fallback_chain.h