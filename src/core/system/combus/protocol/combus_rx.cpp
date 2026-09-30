/******************************************************************************
 * @file combus_rx.cpp
 * @brief ComBus receiver — transport-agnostic implementation.
 *****************************************************************************/

#include "combus_rx.h"

#include <stddef.h>
#include <Arduino.h>

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/combus/protocol/frame/combus_handshake.h>
#include <core/system/combus/protocol/frame/combus_handshake_rx.h>  // combus_handshake_tryDecode
#include <core/system/debug/logging/debug.h>

// RL5 — auto-apply support.  combus_frame_apply() takes a ChanLayer
// caller identity (see combus_frame.h).  No ChanOwner / makeChanOwner
// needed — the layer check is delegated to combus_set_*() inside
// combus_frame_apply().
#include <core/system/combus/combus_defs.h>  // ChanLayer

#if defined(HAS_FAILSAFE)
// P4 — failsafe reaction needs to read the aggregated FAILSAFE channel.
// combus_rx.cpp is a core module and does not own the global comBus
// instance; we declare it extern here, gated by HAS_FAILSAFE so the
// dependency is only pulled in when the feature is compiled in.
// Same pattern as src/core/system/vbat/vbat.cpp:28.
#include <core/config/machines/combus_types.h>  // DigitalComBusID::FAILSAFE (machine family dispatch)
extern ComBus comBus;
#endif



// =============================================================================
// 1. POOL REGISTRY  (externally-owned storage — see combus_rx_register_pool)
// =============================================================================

// Per-link state array — caller-owned static storage, registered at boot
// via combus_rx_register_pool().  Core never allocates this.  The number
// of links is decided by the caller (typically the size of the
// ComBusLink[] array passed to combus_protocol_init_all()); this module
// never imposes a hard cap.
static CombusRxState* s_rxStates = nullptr;
static uint8_t        s_capacity = 0u;

void combus_rx_register_pool( CombusRxState* buffer, uint8_t capacity )
{
    if (s_rxStates != nullptr) {
        sys_log_err("[COMBUS_RX] pool already registered — ignoring second call\n");
        return;
    }
    if (buffer == nullptr || capacity == 0u) {
        sys_log_err("[COMBUS_RX] register_pool called with null buffer or zero capacity\n");
        return;
    }
    s_rxStates = buffer;
    s_capacity = capacity;
}




// =============================================================================
// 2. PER-LINK HANDSHAKE CONTEXT WIRING (P3)
// =============================================================================

void combus_rx_set_handshake_ctx( uint8_t                linkIdx,
                                  CombusHandshakeContext* ctx )
{
    if (!s_rxStates || linkIdx >= s_capacity) { return; }
    s_rxStates[linkIdx].handshakeCtx = ctx;
}




// =============================================================================
// 3. PRIVATE HELPERS  (per-link — take CombusRxState&)
// =============================================================================

/**
 * @brief Append one byte to the ring buffer. Drops oldest byte on overflow.
 */
static void rxBufPush( CombusRxState& st, uint8_t byte )
{
    if (st.rxCount < CombusRxBufSize) {
        uint8_t idx = (uint8_t)((st.rxHead + st.rxCount) % CombusRxBufSize);
        st.rxBuf[idx] = byte;
        st.rxCount++;
    } else {
        // Overflow: drop oldest byte by advancing head
        st.rxBuf[st.rxHead] = byte;
        st.rxHead = (uint8_t)((st.rxHead + 1u) % CombusRxBufSize);
    }
}


/**
 * @brief Return byte at logical index i (0 = oldest).
 */
static uint8_t rxBufAt( const CombusRxState& st, uint8_t i )
{
    return st.rxBuf[(st.rxHead + i) % CombusRxBufSize];
}


/**
 * @brief Discard the n oldest bytes from the ring buffer.
 */
static void rxBufConsume( CombusRxState& st, uint8_t n )
{
    if (n > st.rxCount) { n = st.rxCount; }
    st.rxHead  = (uint8_t)((st.rxHead + n) % CombusRxBufSize);
    st.rxCount = (uint8_t)(st.rxCount - n);
}




/**
 * @brief Attempt to decode a valid frame from the head of the ring buffer.
 *
 * @details Decoding sequence:
 *   1. Scan for SOF — discards leading bytes until `CombusFrameSof` is found
 *      or the buffer is exhausted.
 *   2. Peek the `seq` byte at wire offset 3 (right after the 2-byte
 *      `ComBusFrameCfg` nAnalog/nDigital) to discriminate between a
 *      control frame (`seq` in 1..255) and a handshake frame (`seq == 0`).
 *      The two paths are STRUCTURALLY SEPARATE — the control-frame decoder
 *      is only ever called for non-zero seq, and the handshake stub in
 *      `combus_handshake.cpp` is only ever called for seq == 0.  This keeps
 *      the future versioning logic free to grow inside `combus_handshake.*`
 *      without any refactor of the control-frame path.
 *   3. Control-frame path — reads `nAnalog` / `nDigital` from the wire
 *      header, computes the expected frame length, discards SOF and
 *      re-syncs if the size would exceed `CombusRxBufSize`, copies the frame
 *      into a linear buffer and calls `combus_frame_decode()`.  On CRC
 *      success, updates the snapshot and consumes the frame bytes.  On
 *      CRC failure, discards only the SOF and re-syncs.
 *
 * @return Number of bytes consumed, or 0 if no complete valid frame was found.
 */
static uint8_t tryDecode( CombusRxState& st )
{
    // --- 1. Scan for SOF ---
    while (st.rxCount > 0u && rxBufAt(st, 0u) != CombusFrameSof) {
        rxBufConsume(st, 1u);
    }

    if (st.rxCount < CombusFrameHeaderLen) {
        // Not enough bytes yet even to peek the seq byte — wait for more.
        return 0u;
    }

    // --- 2. Peek `seq` byte (wire offset 3) to discriminate frame kind ---
    uint8_t seqByte = rxBufAt(st, 1u + offsetof(CombusFrameHeader, cfg) +
                                    offsetof(ComBusFrameCfg, nDigital) + 1u);

    if (seqByte == 0u) {
        // --- 2a. HANDSHAKE PATH — structurally separate from control ---
        // Handshake decoder owns its ring-buffer consumption and CRC check.
        // Returning 0 here means "try again next poll" or "no handshake
        // available yet" — the SOF stays in place until either a full
        // handshake frame arrives or the stub decides to drop it.
        return combus_handshake_tryDecode(st.handshakeCtx,
                                          st.rxBuf, CombusRxBufSize,
                                          st.rxHead, st.rxCount);
    }


    // --- 3. CONTROL-FRAME PATH ---
    // From here on, `seq` is guaranteed in 1..255 — no handshake leak possible.
    if (st.rxCount < CombusFrameMinLen) {
        return 0u;
    }

    uint8_t nAnalog   = rxBufAt(st, 1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nAnalog));
    uint8_t nDigital  = rxBufAt(st, 1u + offsetof(CombusFrameHeader, cfg) + offsetof(ComBusFrameCfg, nDigital));
    uint8_t nDigBytes = (nDigital + 7u) / 8u;

    uint16_t expectedLenW = CombusFrameHeaderLen
                          + (uint16_t)nDigBytes
                          + (uint16_t)nAnalog * 2u
                          + 1u;
    if (expectedLenW > CombusRxBufSize) {
        // Impossible frame size — discard SOF and re-sync
        rxBufConsume(st, 1u);
        return 0u;
    }
    uint8_t expectedLen = (uint8_t)expectedLenW;
    if (st.rxCount < expectedLen) {
        return 0u;
    }

    // --- 4. Copy frame into a linear buffer and decode ---
    uint8_t linear[CombusRxBufSize];
    for (uint8_t i = 0u; i < expectedLen; ++i) {
        linear[i] = rxBufAt(st, i);
    }

    // CRC validated before any write — failed decode leaves st.snap untouched.
    // LY2 — decode is layer-agnostic.  It writes by index into the
    // caller-provided buffers without any notion of layer.
    if (combus_frame_decode(st.frameCfg, &st.snap, linear, expectedLen)) {
        st.snapValid    = true;
        st.lastRxMs     = millis();
        st.everReceived = true;

        // RL5 — auto-apply: if the caller wired a target ComBus at init
        // time, push the decoded channels into it now.  This makes
        // combus_frame_apply() a generic capability of the core, triggered
        // automatically by the mere presence of rxCfg + buffers + target.
        // When target == nullptr (test env, legacy callers), no apply is
        // performed — the caller is responsible for calling
        // combus_frame_apply() manually.
        //
        // combus_frame_apply() takes a ChanLayer caller identity (see
        // combus_frame.h).  We pass ChanLayer::REMOTE because the RX
        // path is receiving frames from a remote source — the layer
        // check inside combus_set_*() will accept writes to channels
        // whose declared layer is REMOTE or LOCAL (LOCAL inherits REMOTE).
        if (st.target != nullptr) {
            combus_frame_apply(st.frameCfg, st.target, &st.snap,
                               ChanLayer::REMOTE);
        }

        rxBufConsume(st, expectedLen);
        return expectedLen;
    } else {
        // CRC mismatch — discard SOF and re-sync
        rxBufConsume(st, 1u);
        return 0u;
    }
}




// =============================================================================
// 4. PUBLIC API
// =============================================================================

/**
 * @brief Initialize the ComBus receiver for one link.
 *
 * @details Initialization sequence:
 *   1. Guard check — null transport or buffer pointer, or out-of-range
 *      linkIdx; returns immediately.
 *   2. Store transport interface and frame layout descriptor.
 *   3. Wire caller-provided buffers into the snapshot struct.
 *   4. Reset ring buffer and link state.
 */
void combus_rx_init(
    uint8_t        linkIdx,    // index of this link in the per-link state array
    NodeCom*       nodeCom,    // claimed transport interface (from uart_com_init or similar)
    ComBusFrameCfg frameCfg,   // static frame layout descriptor (nAnalog, nDigital)
    uint16_t*      analogBuf,  // caller-allocated array of frameCfg.nAnalog entries
    bool*          digitalBuf,// caller-allocated array of frameCfg.nDigital entries
    ComBus*        target )    // RL5 — optional apply target (nullptr = no auto-apply)
{
    // --- 1. Guard check ---
    if (!s_rxStates || linkIdx >= s_capacity) { return; }
    if (!nodeCom || !analogBuf || !digitalBuf) { return; }

    CombusRxState& st = s_rxStates[linkIdx];

    // --- 2. Store transport interface and frame layout ---
    st.nodeCom  = nodeCom;
    st.frameCfg = frameCfg;
    st.target   = target;   // RL5 — may be nullptr (legacy / test behaviour)

    // --- 3. Wire caller-provided buffers into the snapshot struct ---
    st.snap.analog  = analogBuf;
    st.snap.digital = digitalBuf;

    // --- 4. Reset ring buffer and link state ---
    st.rxHead       = 0u;
    st.rxCount      = 0u;
    st.snapValid    = false;
    st.everReceived = false;

    // Reset the handshake contract-validated flag too — a fresh RX
    // session must re-validate the MD5+version before any optimisation
    // of subsequent handshake frames kicks in.
    combus_handshake_internal::clearContractValidated(st.handshakeCtx);

    sys_log_info("[COMBUS_RX] init — link=%u  transport='%s'  A%u+D%u\n",
                 (unsigned)linkIdx,
                 nodeCom->name,
                 (unsigned)frameCfg.nAnalog, (unsigned)frameCfg.nDigital);
}



/**
 * @brief Poll transport and decode incoming frames — call every loop iteration.
 *
 * @details Timer-gated, non-blocking — safe to call every loop.  Iterates
 *   over every link registered by combus_rx_register_pool(); each link
 *   has its own transport, ring buffer, and snapshot.
 *   1. Guard check — returns immediately if no pool is registered.
 *   2. Drain available bytes from the transport into the ring buffer.
 *   3. Try to decode frames until the buffer holds fewer than `CombusFrameMinLen`
 *      bytes or `tryDecode()` finds nothing to consume.
 */
void combus_rx_update()
{
    // --- 1. Guard check ---
    if (!s_rxStates) { return; }

    // --- Iterate over every registered link ---
    for (uint8_t i = 0u; i < s_capacity; ++i) {
        CombusRxState& st = s_rxStates[i];

        // --- 2. Per-link guard check ---
        if (!st.nodeCom) { continue; }

#if defined(HAS_FAILSAFE)
        // --- 2b. P4 — Failsafe reaction: invalidate the cached handshake
        //     contract so the next handshake frame is re-validated against
        //     MD5+version.  Without this, a transient link drop leaves the
        //     contractValidated flag stuck at true and the optimisation in
        //     combus_handshake_rx.cpp skips re-validation on reconnect.
        //     Per-link: each link has its own handshakeCtx, so we only
        //     invalidate THIS link's cache — other links are unaffected.
        if (comBus.digitalBus[static_cast<uint8_t>(DigitalComBusID::FAILSAFE)].value) {
            combus_handshake_internal::clearContractValidated(st.handshakeCtx);
        }
#endif

        // --- 3. Drain available bytes into ring buffer ---
        while (st.nodeCom->available(st.nodeCom->ctx) > 0) {
            int b = st.nodeCom->readByte(st.nodeCom->ctx);
            if (b >= 0) { rxBufPush(st, (uint8_t)b); }
        }

        // --- 4. Try to decode (may process multiple back-to-back frames) ---
        while (st.rxCount >= CombusFrameMinLen) {
            if (tryDecode(st) == 0u) {
                break;
            }
        }
    }
}



/** @brief Return latest valid snapshot for one link, or nullptr if no frame received yet. */
const ComBusFrame* combus_rx_snapshot( uint8_t linkIdx )
{
    if (!s_rxStates || linkIdx >= s_capacity) { return nullptr; }
    const CombusRxState& st = s_rxStates[linkIdx];
    return st.snapValid ? &st.snap : nullptr;
}



/** @brief Milliseconds elapsed since the last valid frame on one link, or UINT32_MAX if never received. */
uint32_t combus_rx_age_ms( uint8_t linkIdx )
{
    if (!s_rxStates || linkIdx >= s_capacity) { return UINT32_MAX; }
    const CombusRxState& st = s_rxStates[linkIdx];
    if (!st.everReceived) {
        return UINT32_MAX;
    }
    return (uint32_t)(millis() - st.lastRxMs);
}



/** @brief True if a valid frame was received within the last timeoutMs milliseconds on one link. */
bool combus_rx_is_alive( uint8_t linkIdx, uint32_t timeoutMs )
{
    if (!s_rxStates || linkIdx >= s_capacity) { return false; }
    const CombusRxState& st = s_rxStates[linkIdx];
    return st.everReceived && (combus_rx_age_ms(linkIdx) < timeoutMs);
}

// EOF combus_rx.cpp