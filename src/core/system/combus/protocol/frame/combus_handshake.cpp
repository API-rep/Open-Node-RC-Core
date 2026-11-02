/******************************************************************************
 * @file  combus_handshake.cpp
 * @brief ComBus handshake: shared helpers and changes of the per-link context.
 *
 * @details The RX and TX paths live in their own files (combus_handshake_rx.cpp,
 *   combus_handshake_tx.cpp).  The state lives in CombusHandshakeContext, one
 *   per link: this file holds none.
 *****************************************************************************/

#include "combus_handshake.h"

#include <string.h>
#include <stddef.h>

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/debug/logging/debug.h>


    // The generated header is the source of truth of the wire length.
static_assert(kCombusHandshakePayloadLen ==
                   combus::wire::kCombusHandshakeWirePayloadLen,
               "kCombusHandshakePayloadLen disagrees with "
               "combus::wire::kCombusHandshakeWirePayloadLen.");



// =============================================================================
// 1. SHARED HELPERS
// =============================================================================

void combus_handshake_formatMd5Hex(const uint8_t md5[16], char out[33])
{
    static constexpr char hex[] = "0123456789abcdef";
    for (uint8_t i = 0u; i < 16u; ++i) {
        out[2u * i    ] = hex[(md5[i] >> 4) & 0x0Fu];
        out[2u * i + 1u] = hex[ md5[i]       & 0x0Fu];
    }
    out[32u] = '\0';
}


/*
 * Invoked from the first decode attempt rather than from a global constructor
 * (fragile init order on Arduino).  With the compare disabled, the banner uses
 * repeated markers and uppercase so a `grep` on a saved serial log reveals the
 * bypass at once.
 */
void combus_handshake_logBootWarningIfNeeded(CombusHandshakeContext* ctx)
{
    if (!ctx || ctx->bootWarningLogged || !ctx->expected.stable) { return; }

    char md5Hex[33];
    combus_handshake_formatMd5Hex(ctx->expected.stable, md5Hex);

    if (COMBUS_MD5_CHECK_DISABLE) {
        sys_log_info(
            "\n[COMBUS_HANDSHAKE] ############################################\n"
            "[COMBUS_HANDSHAKE] ##  COMBUS_MD5_CHECK_DISABLE IS ACTIVE        ##\n"
            "[COMBUS_HANDSHAKE] ##  Handshake MD5 comparison is BYPASSED.    ##\n"
            "[COMBUS_HANDSHAKE] ##  Bring-up debug only — no mismatch detect.##\n"
            "[COMBUS_HANDSHAKE] ##  local md5=%s  version=%u.%u               ##\n"
            "[COMBUS_HANDSHAKE] ############################################\n",
            md5Hex,
            (unsigned)combus::wire::kProjectVersionMajor,
            (unsigned)combus::wire::kProjectVersionMinor);
    } else {
        sys_log_info(
            "[COMBUS_HANDSHAKE] md5-check ENABLED  local md5=%s  version=%u.%u\n",
            md5Hex,
            (unsigned)combus::wire::kProjectVersionMajor,
            (unsigned)combus::wire::kProjectVersionMinor);
    }


    ctx->bootWarningLogged = true;
}



// =============================================================================
// 2. STATUS
// =============================================================================

bool combus_handshake_is_contract_validated(const CombusHandshakeContext* ctx) {
    return ctx ? ctx->contractValidated : false;
}



// =============================================================================
// 3. INTERNAL BRIDGE
// =============================================================================

namespace combus_handshake_internal {
    void markEverReceived(CombusHandshakeContext* ctx) {
        if (ctx) { ctx->everReceived = true; }
    }
    void markContractValidated(CombusHandshakeContext* ctx) {
        if (ctx) { ctx->contractValidated = true; }
    }
        // NEXT requests first, when the link has candidates.
    static uint8_t nextTries(const CombusHandshakeContext* ctx) {
        return ctx->hasCandidates ? (uint8_t)(kCombusHandshakeBurstCount + kCombusHandshakeNextRetries) : 0u;
    }

    void clearContractValidated(CombusHandshakeContext* ctx) {
        if (!ctx) { return; }
        ctx->contractValidated = false;
        ctx->ackPending        = false;
        ctx->ackPendingNext    = false;
        ctx->dataSinceStable   = false;
        ctx->refusalLogged     = false;
        ctx->nextMode          = false;
        ctx->nextTriesLeft     = nextTries(ctx);
    }
    void dropContract(CombusHandshakeContext* ctx, const char* reason) {
        if (!ctx || !ctx->contractValidated) { return; }
        ctx->contractValidated = false;
        ctx->dataSinceStable   = false;
        ctx->nextMode          = false;
        ctx->nextTriesLeft     = nextTries(ctx);
        sys_log_warn("[COMBUS_HANDSHAKE] link '%s' contract UNKNOWN (%s)\n",
                     ctx->name ? ctx->name : "?", reason);
    }

    void startBurst(CombusHandshakeContext* ctx) {
        if (!ctx) { return; }
        ctx->burstActive    = true;
        ctx->burstRemaining = kCombusHandshakeBurstCount;
        ctx->lastBurstMs    = 0u;  // 0: first emission on the next update
        ctx->lastRequestMs  = 0u;  // the periodic requests count from the end of the burst
    }
    void stopBurst(CombusHandshakeContext* ctx) {
        if (!ctx) { return; }
        ctx->burstActive    = false;
        ctx->burstRemaining = 0u;
    }
}

// EOF combus_handshake.cpp
