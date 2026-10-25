/******************************************************************************
 * @file combus_protocol.cpp
 * @brief ComBus protocol layer — transport-agnostic implementation.
 *****************************************************************************/

#include "combus_protocol.h"

#include <string.h>   // memcmp()

#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/combus/protocol/frame/combus_handshake.h>  // CombusHandshakeContext
#include <core/system/combus/protocol/frame/combus_handshake_rx.h>  // (no-op, ensures include path)
#include <core/system/debug/logging/debug.h>                       // sys_log_err



// =============================================================================
// 1. COMBUS PROTOCOL INIT — PER-LINK LOOP
// =============================================================================
//
//   combus_protocol_init_all() does NOT allocate any state.  The caller
//   (board) is responsible for declaring its own static CombusTxState[]
//   and CombusRxState[] pools and registering them via
//   combus_tx_register_pool() and combus_rx_register_pool() BEFORE
//   calling this function.  This module only iterates over the
//   caller-owned ComBusLink[] array and wires each link to its slot in
//   the registered pools.
//
// =============================================================================


void combus_protocol_init_all( ComBusLink* links, uint8_t count )
{
        // --- Initialise every link in the caller-owned array ---
    for (uint8_t i = 0u; i < count; ++i) {
        combus_protocol_init(i, &links[i]);
    }
}


// =============================================================================
// 2. COMBUS PROTOCOL INIT — PER-LINK
// =============================================================================


void combus_protocol_init( uint8_t linkIdx, ComBusLink* link )
{
        // --- 1. Null guard ---
    if (!link || !link->com) { return; }
    if (!link->handshakeCtx) {
        sys_log_err("[COMBUS_PROTOCOL] init rejected — null handshakeCtx for link '%s'\n",
                    link->name);
        return;
    }

        // --- 2. LY4 — resolve the expected MD5 pointer from link->layer.
        //    Same pattern as LY2 wireEnd counters: resolved once at init,
        //    stored in the per-link handshake context, read by both TX
        //    (to embed in the magic frame) and RX (to compare against
        //    the on-wire MD5).  No switch repeated at every frame.
        //
        //    Only BASE, SYSTEM and BOARD are valid for a handshake link.
        //    UNDEFINED is a config error (forgotten / missing layer) and
        //    triggers a FATAL halt — a silent rejection would mask the
        //    bug instead of revealing it (same pattern as the
        //    "no pool registered" FATAL in uart_com.cpp and the
        //    UNDEFINED FATAL in combus_tx_init()).
    switch (link->layer) {
        case ChanLayer::BASE:
            link->handshakeCtx->expectedMd5 = combus::wire::kCombusBaseComBusMd5;
            break;
        case ChanLayer::SYSTEM:
            link->handshakeCtx->expectedMd5 = combus::wire::kCombusSystemComBusMd5;
            break;
        case ChanLayer::BOARD:
            // BOARD — reserved for a future intra-device loopback.
            // Falls back to the FULL view (machine node default).
            link->handshakeCtx->expectedMd5 = combus::wire::kCombusComBusMd5;
            break;
        case ChanLayer::UNDEFINED:
        default:
            // UNDEFINED — config error (forgotten / missing layer).
            // FATAL halt: a silent rejection would mask the bug instead
            // of revealing it.
            sys_log_err("[COMBUS_PROTOCOL] FATAL: ChanLayer::UNDEFINED is not a valid layer for link '%s' — system halted\n",
                        link->name);
            while (1) { /* halt */ }
            break;
    }

        // --- 3. Wire the per-link handshake context BEFORE init so the
        //    burst is armed on combus_tx_init() and the contract-
        //    validated flag is shared between TX and RX.  P3.
    combus_tx_set_handshake_ctx(linkIdx, link->handshakeCtx);
    combus_rx_set_handshake_ctx(linkIdx, link->handshakeCtx);

        // --- 3b. Chantier 1.2 — the contract rules depend on the ends of the
        //    link: a bidirectional end answers requests, waits for an ACK and
        //    sends no data while the contract is UNKNOWN.
    link->handshakeCtx->name          = link->name;
    link->handshakeCtx->bidirectional = link->txCfg.has_value() && link->rxCfg.has_value();

        // --- 3c. Chantier 2.2b — next set: only a BASE link has candidates apart
        //    (a SYSTEM link carries them in its single set).  Without candidate,
        //    the next MD5 is the stable one and the link is legacy only.
    link->handshakeCtx->baseSet         = (link->layer == ChanLayer::BASE);
    link->handshakeCtx->expectedMd5Next = link->handshakeCtx->baseSet
        ? combus::wire::kCombusBaseComBusMd5Next
        : link->handshakeCtx->expectedMd5;

        // --- 3d. Link rework 3.1b — a link between nodes has its own contract:
        //    the MD5 of the link, computed by the builder from both types.
    link->handshakeCtx->peer = link->peer;
    if (link->peer) {
        link->handshakeCtx->expectedMd5     = link->peer->md5Stable;
        link->handshakeCtx->expectedMd5Next = link->peer->md5;
    }
    link->handshakeCtx->hasCandidates   =
        memcmp(link->handshakeCtx->expectedMd5Next, link->handshakeCtx->expectedMd5, 16) != 0;

    combus_handshake_internal::clearContractValidated(link->handshakeCtx);   // every end starts UNKNOWN

        // --- 4. TX protocol layer — only if txCfg is present ---
        //    LY2 — link->layer is passed so combus_tx_init() can
        //    resolve analogWireEnd / digitalWireEnd from the matching
        //    view's CH_COUNT.
    if (link->txCfg) {
        combus_tx_init(linkIdx, link->com, *link->txCfg, link->txHz, link->layer);
    }

        // --- 5. RX protocol layer — only if rxCfg is present ---
    if (link->rxCfg) {
        combus_rx_init(linkIdx, link->com, *link->rxCfg, link->analogBuf, link->digitalBuf, link->target, link->layer);
    }
}


// EOF combus_protocol.cpp