/******************************************************************************
 * @file  combus_handshake.h
 * @brief ComBus handshake frame handling — RESERVED wire slot, NOT YET
 *        implemented on the wire.
 *
 * @details The ComBus rolling sequence counter (`seq` field in
 *   `CombusFrameHeader`) is reserved to the range **1..255** for normal
 *   control frames.  The value **`seq == 0` is RESERVED** for a future
 *   handshake / versioning frame that will:
 *     - detect accidental version / layout mismatches between any two
 *       ComBus participants (machine <-> remote over RF, or mainboard
 *       <-> extension board over serial);
 *     - live on the high-priority QoS channel once that layer exists.
 *
 *   This header declares the dedicated dispatcher entry point used by the
 *   RX decoder.  It is **structurally separate** from the regular control-
 *   frame decode path so that the future handshake implementation can be
 *   added here without any refactor of `combus_frame.{h,cpp}`.
 *
 *   @note The handshake itself is NOT implemented in this revision.  The
 *     stub below only validates the structural shape of the frame (CRC,
 *     fixed payload length) and consumes the bytes from the RX ring buffer
 *     so the link stays re-synchronised.  No payload interpretation, no
 *     friend-cache update, no MD5 check — out of scope.
 *
 *   Generic to every ComBus participant (RF node or serial board), NOT
 *   limited to the machine <-> remote link.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/frame/combus_frame.h>   // CombusFrameHeader, CombusFrameSof
#include <core/system/hw/node_com.h>                // NodeCom (TX path)

// Auto-generated per-machine-type MD5 header (scripts/combus_md5.py).
// Today: only dumper_truck exposes a REMOTE-ID pair, so the handshake
// umbrella references the dumper_truck-generated header directly.  When a
// second machine type ships, this include will move to a TYPE-dispatched
// location (machine_type.h) and route to the right pair.
#include <core/config/machines/dumper_truck/combus/combus_ids_remote_md5.h>


// Side-specific entry points are declared in:
//   combus_handshake_rx.h  — combus_handshake_tryDecode()
//   combus_handshake_tx.h  — combus_handshake_sendOnce()
// Include them as needed; this umbrella is intentionally neutral.



// =============================================================================
// 1. WIRE CONSTANTS — HANDSHAKE FRAME
// =============================================================================

/**
 * @brief Start-of-frame sentinel byte for a handshake frame.
 *
 * @details Currently identical to the control-frame SOF (`CombusFrameSof`)
 *   — the discrimination is done on the `seq` byte that immediately follows
 *   the fixed 3-byte `ComBusFrameCfg` (nAnalog, nDigital, seq).
 *
 *   When the handshake gets its own dedicated SOF in the future (e.g. to
 *   allow coexistence with discovery / scan traffic on a shared bus), this
 *   constant is the natural place to change.
 */
static constexpr uint8_t CombusHandshakeSof = CombusFrameSof;  ///< placeholder

/**
 * @brief Fixed payload length of the handshake frame, in bytes.
 *
 * @details Excludes SOF, header and CRC-8.
 *
 *   Handshake payload layout (kCombusHandshakeWirePayloadLen = 18):
 *     [0..15]   MD5 of the Remote-only ComBus .inc files for the active
 *               MACHINE_TYPE_*.  See scripts/combus_md5.py for the hash
 *               inputs and the deterministic ordering.
 *     [16]      Project version MAJOR (see include/project_version.h).
 *     [17]      Project version MINOR.
 *
 *   The MD5 + version pair is sufficient to detect any accidental mismatch
 *   between two ComBus participants on the wire.  The "friend cache" /
 *   sender-address layer is OUT OF SCOPE of this revision (planned for a
 *   future commit) and will ride on top of this same payload slot.
 *
 *   Total wire bytes for a handshake frame = 1 (SOF) +
 *   `sizeof(CombusFrameHeader)` + `kCombusHandshakePayloadLen` + 1 (CRC-8).
 *
 *   @note `kCombusHandshakePayloadLen = 18` is the wire contract.  It is
 *     duplicated locally (not pulled from the auto-generated
 *     `combus_ids_remote_md5.h` namespace) so the umbrella compiles even
 *     if a future machine type has no MD5 header yet.  A compile-time
 *     `static_assert` at the bottom of combus_handshake.cpp enforces the
 *     match against the generated `kCombusWireMd5[16]` length.

 */
static constexpr uint8_t kCombusHandshakePayloadLen = 18u;   // 16 (md5) + 1 (major) + 1 (minor)


/// Total handshake frame size on the wire (SOF + header + payload + CRC).
static constexpr uint8_t CombusFrameHandshakeMinLen =
    sizeof(CombusFrameSof) + sizeof(CombusFrameHeader) +
    kCombusHandshakePayloadLen + sizeof(uint8_t);



// =============================================================================
// 2. SHARED (RX + TX) BOOT-WARNING HELPER
// =============================================================================

/**
 * @brief Emit the boot-time MD5-compare / version banner exactly once.
 *
 * @details Lives in the umbrella header so both RX and TX TUs can call it
 *   from their first invocation (the umbrella itself does not carry a
 *   transport handle, so it cannot log directly).
 *
 *   Idempotent — guarded by an internal static flag.
 */
void combus_handshake_logBootWarningIfNeeded();

/**
 * @brief Format a 16-byte MD5 as a 32-char lowercase hex string into `out`.
 *
 * @details `out` MUST have room for at least 33 bytes (32 hex chars + NUL).
 *   Always NUL-terminates.  Used by both the boot banner and the RX/TX
 *   log lines so a single canonical representation is produced across the
 *   whole module.
 *
 * @param[in]  md5  Pointer to 16 bytes (the MD5).
 * @param[out] out  Caller-owned buffer, must be >= 33 bytes.
 */
void combus_handshake_formatMd5Hex(const uint8_t md5[16], char out[33]);


// =============================================================================
// 3. PUBLIC STATUS HELPERS
// =============================================================================

/**
 * @brief True if at least one valid handshake frame has ever been observed
 *   since boot, on any ComBus transport the node participates in.
 *
 * @details Mirrors `combus_rx_ever_received()` for control frames.  Exposed
 *   as a thin accessor so the future cache / versioning layer can poll
 *   liveness without including the full RX module.
 */
bool combus_handshake_ever_received();


// =============================================================================
// 4. RX / TX ENTRY POINTS — declared in the side-specific headers:
//      - combus_handshake_rx.h   (combus_handshake_tryDecode)
//      - combus_handshake_tx.h   (combus_handshake_sendOnce)
//    Include them directly where needed.


// =============================================================================
// 5. INTERNAL BRIDGE — written by RX, read by the umbrella
// =============================================================================

/**
 * @brief Cross-TU helper namespace used by combus_handshake_rx.cpp to set
 *        the shared `s_handshakeEverReceived` flag owned by the umbrella.
 *        Not part of the public API — never call from outside the
 *        handshake module.
 */
namespace combus_handshake_internal {
    void markEverReceived();
}

// EOF combus_handshake.h

