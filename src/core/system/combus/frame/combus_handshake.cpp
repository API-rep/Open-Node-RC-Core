/******************************************************************************
 * @file  combus_handshake.cpp
 * @brief ComBus handshake umbrella — shared helpers (boot banner + MD5 hex
 *        formatting + status flag).
 *
 * @details The RX and TX paths live in their own translation units
 *   (combus_handshake_rx.cpp, combus_handshake_tx.cpp) — mirror of the
 *   combus_rx.cpp / combus_tx.cpp split.  This .cpp owns only the cross-
 *   cutting state and helpers shared by both sides:
 *
 *     - s_handshakeEverReceived          (status flag)
 *     - s_bootWarningLogged              (one-shot flag for the boot banner)
 *     - combus_handshake_logBootWarningIfNeeded()
 *     - combus_handshake_ever_received()
 *     - combus_handshake_formatMd5Hex()  (32-char hex writer, used by both
 *                                          RX compare and TX build paths)
 *
 *   Payload layout and side contracts live in:
 *     - combus_handshake.h        (umbrella)
 *     - combus_handshake_rx.h/cpp (decode + CRC + compare)
 *     - combus_handshake_tx.h/cpp (frame build + sendOnce)
 *
 *   Scope EXPLICITLY out of this revision (to be added later):
 *     - friend-cache update ("amies" / sender address);
 *     - automatic burst at boot;
 *     - high-priority QoS routing.
 *****************************************************************************/

#include "combus_handshake.h"

#include <string.h>
#include <stddef.h>

#include <core/system/combus/frame/combus_frame.h>
#include <core/system/debug/logging/debug.h>


// =============================================================================
// 0. COMPILE-TIME GUARDS
// =============================================================================

// Handshake payload length MUST equal what the auto-generated header
// advertises as the wire length.  This is the SOLE guard — the previous
// `static_assert(..., 18u)` was redundant with the one below and has been
// removed: 18u is the wire magic that combus_md5.py writes, but the
// authoritative source of truth lives in the generated header itself.
static_assert(kCombusHandshakePayloadLen ==
                   combus::wire::kCombusHandshakeWirePayloadLen,
               "kCombusHandshakePayloadLen disagrees with "
               "combus::wire::kCombusHandshakeWirePayloadLen.");

// COMBUS_MD5_CHECK_DISABLE is defined in combus_handshake.h (umbrella) as
// the single source of truth — see R1.4 in WIP combus_v2 §6.



// =============================================================================
// 1. PERSISTENT STATE
// =============================================================================
//
// P3 — handshake state is per ComBus link instance, NOT a static global.
// The umbrella helpers below take a `CombusHandshakeContext*` and read /
// write the corresponding fields.  See combus_handshake.h §5 for the
// rationale (multiple independent ComBus interfaces per machine).
//
// The boot-warning helper is the only one that still uses a static guard
// (s_bootWarningLogged) — it is a one-shot log line, not per-link state.
// The guard is moved into the context so it is also per-link; the umbrella
// helper takes the context as a parameter.




// =============================================================================
// 2. SHARED HELPERS
// =============================================================================

/**
 * @brief Format a 16-byte MD5 as a 32-char lowercase hex string into `out`.
 *
 * @details `out` MUST have room for at least 33 bytes (32 hex chars + NUL).
 *   Always NUL-terminates.  Used by both the boot banner and the RX/TX
 *   log lines so a single canonical representation is produced across the
 *   whole module.
 *
 *   Kept header-only-ish (this is the .cpp definition) so the binary
 *   cost is paid once, but the helper stays callable from any side TU
 *   via combus_handshake.h.
 *
 * @param[in]  md5  Pointer to 16 bytes (the MD5).
 * @param[out] out  Caller-owned buffer, must be >= 33 bytes.
 */
void combus_handshake_formatMd5Hex(const uint8_t md5[16], char out[33])
{
    static constexpr char hex[] = "0123456789abcdef";
    for (uint8_t i = 0u; i < 16u; ++i) {
        out[2u * i    ] = hex[(md5[i] >> 4) & 0x0Fu];
        out[2u * i + 1u] = hex[ md5[i]       & 0x0Fu];
    }
    out[32u] = '\0';
}


/**
 * @brief Emit an impossible-to-miss boot banner when MD5 check is disabled.
 *
 * @details Invoked from the first RX or TX call (cheap flag-guarded).  We
 *   do NOT rely on a global ctor (fragile init order on Arduino) — the
 *   idempotence is owned by `ctx->bootWarningLogged` (per-link).
 *
 *   The banner uses repeated markers + uppercase so a quick `grep` on a
 *   saved serial log immediately reveals the bypass is active.
 *
 * @param ctx  Per-link handshake context (must not be null).
 */
void combus_handshake_logBootWarningIfNeeded(CombusHandshakeContext* ctx)
{
    if (!ctx || ctx->bootWarningLogged) { return; }

    char md5Hex[33];
    combus_handshake_formatMd5Hex(combus::wire::kCombusWireMd5, md5Hex);

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
// 3. PUBLIC STATUS HELPER
// =============================================================================

bool combus_handshake_ever_received(const CombusHandshakeContext* ctx) {
    return ctx ? ctx->everReceived : false;
}

bool combus_handshake_is_contract_validated(const CombusHandshakeContext* ctx) {
    return ctx ? ctx->contractValidated : false;
}



// =============================================================================
// 4. BRIDGE TO THE RX / TX TUs
// =============================================================================
//
// All setters take a `CombusHandshakeContext*` and mutate the per-link
// state.  Null pointer is a no-op (defensive — the umbrella helpers are
// called from many places).

namespace combus_handshake_internal {
    void markEverReceived(CombusHandshakeContext* ctx) {
        if (ctx) { ctx->everReceived = true; }
    }
    void markContractValidated(CombusHandshakeContext* ctx) {
        if (ctx) { ctx->contractValidated = true; }
    }
    void clearContractValidated(CombusHandshakeContext* ctx) {
        if (ctx) { ctx->contractValidated = false; }
    }
}




// EOF combus_handshake.cpp