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

#ifndef COMBUS_MD5_CHECK_DISABLE
  /// @brief Compile-time switch — when defined (non-zero), MD5 compare is
  ///        short-circuited.  Logged loudly at boot.  Default OFF.
  #define COMBUS_MD5_CHECK_DISABLE  0
#endif


// =============================================================================
// 1. PERSISTENT STATE
// =============================================================================

/**
 * @brief Persistent handshake-liveness flag.
 *
 * @details Tracked separately from the control-frame `everReceived` so the
 *   future versioning layer can tell control traffic from handshake traffic
 *   without inspecting the snapshot itself.
 *
 *   Lifetime: static — valid for the entire program run.
 */
static bool s_handshakeEverReceived = false;

/// One-shot guard for the boot banner — see combus_handshake.h.
static bool s_bootWarningLogged = false;


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
 *   idempotence is owned by `s_bootWarningLogged`.
 *
 *   The banner uses repeated markers + uppercase so a quick `grep` on a
 *   saved serial log immediately reveals the bypass is active.
 */
void combus_handshake_logBootWarningIfNeeded()
{
    if (s_bootWarningLogged) { return; }

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
            (unsigned)combus::wire::kCombusWireVersionMajor,
            (unsigned)combus::wire::kCombusWireVersionMinor);
    } else {
        sys_log_info(
            "[COMBUS_HANDSHAKE] md5-check ENABLED  local md5=%s  version=%u.%u\n",
            md5Hex,
            (unsigned)combus::wire::kCombusWireVersionMajor,
            (unsigned)combus::wire::kCombusWireVersionMinor);
    }

    s_bootWarningLogged = true;
}


// =============================================================================
// 3. PUBLIC STATUS HELPER
// =============================================================================

bool combus_handshake_ever_received() {
    return s_handshakeEverReceived;
}


// =============================================================================
// 4. BRIDGE TO THE RX / TX TUs
// =============================================================================
//
// `s_handshakeEverReceived` is defined here but written by the RX TU.
// Forward-declare a tiny setter that lives next to the compare path.

namespace combus_handshake_internal {
    void markEverReceived();   // defined in combus_handshake_rx.cpp
}

namespace combus_handshake_internal {
    bool g_everReceived() { return s_handshakeEverReceived; }
    void markEverReceived() { s_handshakeEverReceived = true; }
}


// EOF combus_handshake.cpp