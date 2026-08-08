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



// Forward declaration — the full definition lives in §3 below.  Needed
// so the boot-warning helper (§2) can take a `CombusHandshakeContext*`
// parameter without a circular include.
struct CombusHandshakeContext;



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
// 1b. BURST-AT-BOOT CONSTANTS (P3)
// =============================================================================

/**
 * @brief Number of handshake frames emitted in the boot-time burst.
 *
 * @details P3 (WIP combus_v2 §3) — the TX side emits a short burst of
 *   handshake frames at boot so the peer can validate the contract even
 *   if the very first frame is lost on a noisy link.  3 is the minimum
 *   viable count per the WIP ("rafale courte 3–5 répétitions").
 *
 *   Per-link state — see CombusHandshakeContext.  This constant is the
 *   shared default; each link instance starts its burst counter at this
 *   value.
 */
static constexpr uint8_t kCombusHandshakeBurstCount = 3u;

/**
 * @brief Interval between two consecutive handshake burst emissions, in ms.
 *
 * @details P3 — 100 ms gives the peer ~10 chances per second to receive
 *   and validate the contract during the burst window.  Adjustable after
 *   peer review (see WIP combus_v2 §5.2 P3 row).
 */
static constexpr uint32_t kCombusHandshakeBurstPeriodMs = 100u;



// =============================================================================
// 1c. COMPILE-TIME SWITCHES — UNIQUE DEFINITIONS
// =============================================================================

/**
 * @brief Compile-time switch — when defined (non-zero), the MD5+version
 *        compare on the RX path is short-circuited.  Logged loudly at boot.
 *        Default OFF.
 *
 * @details Defined HERE (umbrella) as the single source of truth.  Side
 *   headers (combus_handshake_rx.h, combus_handshake_tx.h) MUST NOT
 *   redefine this — they include the umbrella and pick up the value.
 *   See R1.4 in the WIP combus_v2 §6 backlog.
 */
#ifndef COMBUS_MD5_CHECK_DISABLE
  #define COMBUS_MD5_CHECK_DISABLE  0
#endif

/**
 * @brief Compile-time switch — when defined (non-zero), the boot-time
 *        handshake burst is disabled.  Logged loudly at boot.  Default OFF.
 *
 * @details P3 — debug-only switch, mirror of COMBUS_MD5_CHECK_DISABLE.
 *   When active, the TX side never emits the burst; the contract must
 *   be validated by the peer sending a handshake frame first.
 */
#ifndef COMBUS_HANDSHAKE_BURST_DISABLE
  #define COMBUS_HANDSHAKE_BURST_DISABLE  0
#endif




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
 *   Idempotent — guarded by `ctx->bootWarningLogged` (per-link).
 *
 * @param ctx  Per-link handshake context (must not be null).
 */
void combus_handshake_logBootWarningIfNeeded(CombusHandshakeContext* ctx);


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
// 3. PER-LINK HANDSHAKE CONTEXT (P3)
// =============================================================================
//
// Declared BEFORE the public status helpers so the helpers can take a
// `const CombusHandshakeContext*` parameter without a forward declaration.

/**
 * @brief Per-ComBus-link handshake state container.
 *
 * @details Handshake state is per ComBus link instance.  Multiple
 *   independent ComBus interfaces may coexist in the same machine
 *   (e.g. ESP/network + UART/extension board).  Handshake validation
 *   and TX burst state MUST NEVER be shared between links.
 *
 *   This is a fundamental property of the ComBus model, not an
 *   anticipation of N2 (asymmetric transports).
 *
 *   Lifetime: caller-owned (typically embedded in CombusTxState /
 *   CombusRxState, or held by the environment that owns the link).
 *   Zero-initialised at construction — all flags start `false`,
 *   counters start at 0.
 *
 *   Thread-safety: not thread-safe.  ComBus is single-threaded on
 *   the targets it runs on (ESP32 Arduino core, etc.).
 */
struct CombusHandshakeContext {
    // --- RX-side state (P2, migrated from static globals) ---
    bool     contractValidated = false;  ///< true after first MD5+version match
    bool     everReceived      = false;  ///< true after first valid handshake frame observed
    bool     bootWarningLogged = false;  ///< one-shot guard for the boot banner

    // --- TX-side state (P3) ---
    bool     burstActive       = false;  ///< true while the boot-time burst is in progress
    uint8_t  burstRemaining    = 0u;     ///< frames left to emit in the current burst
    uint32_t lastBurstMs       = 0u;     ///< millis() at the last burst emission
};



// =============================================================================
// 4. PUBLIC STATUS HELPERS
// =============================================================================

/**
 * @brief True if at least one valid handshake frame has ever been observed
 *   on the given ComBus link since the last transport reset.
 *
 * @details Per-link state — see CombusHandshakeContext.  Mirrors
 *   `combus_rx_ever_received()` for control frames.  Exposed as a thin
 *   accessor so the future cache / versioning layer can poll liveness
 *   without including the full RX module.
 *
 * @param ctx  Per-link handshake context (must not be null).
 */
bool combus_handshake_ever_received(const CombusHandshakeContext* ctx);

/**
 * @brief True if the local contract (MD5 + project version) has been
 *   validated against a peer on the given ComBus link since the last
 *   transport reset.
 *
 * @details Per-link state — see CombusHandshakeContext.  Set to `true`
 *   by the RX path on the first handshake frame whose MD5 + version
 *   match the locally-generated copy.  Reset to `false` by
 *   `combus_handshake_internal::clearContractValidated()` — called from
 *   `combus_rx_init()` so the flag is automatically cleared whenever the
 *   transport is (re)initialised.
 *
 *   Intended use: skip the MD5+version compare on subsequent handshake
 *   frames once the contract is known to be valid, while still running
 *   every other frame-level check (CRC, length, format).  Does NOT
 *   short-circuit any control-frame validation.
 *
 *   Independent from `combus_handshake_ever_received()`: the latter is
 *   true as soon as ANY handshake frame is observed (match or not), the
 *   former only on a successful match.
 *
 * @param ctx  Per-link handshake context (must not be null).
 */
bool combus_handshake_is_contract_validated(const CombusHandshakeContext* ctx);




// =============================================================================
// 5. RX / TX ENTRY POINTS — declared in the side-specific headers:
//      - combus_handshake_rx.h   (combus_handshake_tryDecode)
//      - combus_handshake_tx.h   (combus_handshake_sendOnce)
//    Include them directly where needed.



// =============================================================================
// 6. INTERNAL BRIDGE — written by RX, read by the umbrella
// =============================================================================

/**
 * @brief Cross-TU helper namespace used by combus_handshake_rx.cpp to
 *        mutate the per-link context owned by the caller.  Not part of
 *        the public API — never call from outside the handshake module.
 *
 * @details All functions take a `CombusHandshakeContext*` so the state
 *   is per-link.  A null pointer is treated as a no-op (defensive —
 *   the umbrella helpers are called from many places).
 */
namespace combus_handshake_internal {
    void markEverReceived(CombusHandshakeContext* ctx);

    /**
     * @brief Mark the local contract as validated against a peer on the
     *        wire.  Called from `combus_handshake_rx.cpp` after a
     *        successful MD5+version match.
     */
    void markContractValidated(CombusHandshakeContext* ctx);

    /**
     * @brief Clear the contract-validated flag.  Called from
     *        `combus_rx_init()` so the flag is automatically reset
     *        whenever the transport is (re)initialised.
     */
    void clearContractValidated(CombusHandshakeContext* ctx);

    /**
     * @brief Start (or restart) the boot-time handshake burst on the
     *        given link.  Sets `burstActive = true`, `burstRemaining =
     *        kCombusHandshakeBurstCount`, `lastBurstMs = 0` so the
     *        first emission happens on the very next `tx_update()` call.
     *
     * @details Called from `combus_tx_init()` so the burst is armed
     *   whenever the transport is (re)initialised.  Idempotent — calling
     *   twice in a row is equivalent to calling once.
     */
    void startBurst(CombusHandshakeContext* ctx);

    /**
     * @brief Stop the boot-time handshake burst on the given link.
     *        Sets `burstActive = false`, `burstRemaining = 0`.
     *
     * @details Called from the RX path when the contract is validated
     *   (early termination — see P3 constraint #3) and from
     *   `combus_tx_init()` to reset state on transport reinit.
     */
    void stopBurst(CombusHandshakeContext* ctx);
}


// EOF combus_handshake.h




