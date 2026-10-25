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

            // --- 4b. Chantier 1.2 — contract UNKNOWN: a valid frame is dropped.
            //    Not applied, snapshot invalidated, and it does not make the link
            //    alive: a peer with another contract counts as never seen.  A link
            //    without handshake context (codec tests, legacy callers) has no
            //    contract.
        if (st.handshakeCtx && !st.handshakeCtx->contractValidated) {
            st.snapValid = false;
            rxBufConsume(st, expectedLen);
            return expectedLen;
        }

        st.snapValid    = true;
        st.lastRxMs     = millis();
        st.everReceived = true;
        if (st.handshakeCtx) { st.handshakeCtx->dataSinceStable = true; }

        // RL5 — auto-apply: if the caller wired a target ComBus at init
        // time, push the decoded channels into it now.  This makes
        // combus_frame_apply() a generic capability of the core, triggered
        // automatically by the mere presence of rxCfg + buffers + target.
        // When target == nullptr (test env, legacy callers), no apply is
        // performed — the caller is responsible for calling
        // combus_frame_apply() manually.
        //
        // combus_frame_apply() takes a ChanLayer caller identity (see
        // combus_frame.h).  The identity is the layer of the link that
        // received the frame (st.writeLayer): the layer check inside
        // combus_set_*() lets a BASE link write BASE channels only, and a
        // SYSTEM link write BASE and SYSTEM channels.  A frame can never
        // write a BOARD channel (see combus_rx_init()).
        if (st.target != nullptr) {
                // Chantier 2.2b — a BASE link out of next mode applies the stable
                // channels only: the candidates keep their default value.
                // Link rework 3.1b — a link between nodes: its own stable counts,
                // and its table from the wire positions to this node's channels.
            const ComPeerLink* peer = st.handshakeCtx ? st.handshakeCtx->peer : nullptr;
            ComBusFrameCfg applyCfg = st.frameCfg;
            if (st.handshakeCtx && st.handshakeCtx->baseSet && !st.handshakeCtx->nextMode) {
                applyCfg.nAnalog  = peer ? peer->nStableAnalog  : combus::wire::kCombusBaseStableAnalogCount;
                applyCfg.nDigital = peer ? peer->nStableDigital : combus::wire::kCombusBaseStableDigitalCount;
            }
            combus_frame_apply(applyCfg, st.target, &st.snap,
                               st.writeLayer, peer);
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
 *   3. Resolve the write identity used to apply frames from the link layer.
 *   4. Wire caller-provided buffers into the snapshot struct.
 *   5. Reset ring buffer and link state.
 */
void combus_rx_init(
    uint8_t        linkIdx,    // index of this link in the per-link state array
    NodeCom*       nodeCom,    // claimed transport interface (from uart_com_init or similar)
    ComBusFrameCfg frameCfg,   // static frame layout descriptor (nAnalog, nDigital)
    uint16_t*      analogBuf,  // caller-allocated array of frameCfg.nAnalog entries
    bool*          digitalBuf,// caller-allocated array of frameCfg.nDigital entries
    ComBus*        target,     // RL5 — optional apply target (nullptr = no auto-apply)
    ChanLayer      layer )     // layer of the link — write identity of the applied frames
{
    // --- 1. Guard check ---
    if (!s_rxStates || linkIdx >= s_capacity) { return; }
    if (!nodeCom || !analogBuf || !digitalBuf) { return; }

    CombusRxState& st = s_rxStates[linkIdx];

    // --- 2. Store transport interface and frame layout ---
    st.nodeCom  = nodeCom;
    st.frameCfg = frameCfg;
    st.target   = target;   // RL5 — may be nullptr (legacy / test behaviour)

    // --- 3. Resolve the write identity from the link layer.
    //    A frame received on a link may only write the channels that link
    //    is allowed to carry: BASE link -> BASE channels, SYSTEM link ->
    //    BASE and SYSTEM channels.  A BOARD channel is private to one
    //    board and must never be written from the wire, so a BOARD link
    //    (reserved loopback) is given the SYSTEM identity.
    //    The layer only matters when frames are applied (target != nullptr).
    //    UNDEFINED is then a config error (forgotten / missing layer) and
    //    triggers a FATAL halt — a silent fallback would mask the bug
    //    (same pattern as combus_tx_init()).
    st.layer      = layer;
    st.writeLayer = (layer == ChanLayer::BOARD) ? ChanLayer::SYSTEM : layer;
    if (target != nullptr && layer == ChanLayer::UNDEFINED) {
        sys_log_err("[COMBUS_RX] FATAL: ChanLayer::UNDEFINED is not a valid layer for a link applying frames — system halted\n");
        while (1) { /* halt */ }
    }

    // --- 4. Wire caller-provided buffers into the snapshot struct ---
    st.snap.analog  = analogBuf;
    st.snap.digital = digitalBuf;

    // --- 5. Reset ring buffer and link state ---
    st.rxHead       = 0u;
    st.rxCount      = 0u;
    st.snapValid    = false;
    st.everReceived = false;

    // Reset the handshake contract-validated flag too — a fresh RX
    // session must re-validate the MD5+version before any optimisation
    // of subsequent handshake frames kicks in.
    combus_handshake_internal::clearContractValidated(st.handshakeCtx);

    sys_log_info("[COMBUS_RX] init — link=%u  transport='%s'  A%u+D%u  layer=%u\n",
                 (unsigned)linkIdx,
                 nodeCom->name,
                 (unsigned)frameCfg.nAnalog, (unsigned)frameCfg.nDigital,
                 (unsigned)layer);
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
 *
 *   4. Chantier 1.2 — drop a STABLE contract back to UNKNOWN after a silence
 *      longer than the link timeout (once data has flowed), or, on a
 *      bidirectional end, after kCombusHandshakeAckTimeoutMs without ACK.
 *
 *   @p bus is no longer read: the FAILSAFE reaction (P4), which dropped the
 *   contract while FAILSAFE was active, is removed (chantier 1.2) — with the
 *   contract blocking the frames, it would block the FAILSAFE of the peers.
 */
void combus_rx_update(const ComBus* bus)
{
    // --- 1. Guard check ---
    if (!s_rxStates) { return; }
    (void)bus;

    // --- Iterate over every registered link ---
    for (uint8_t i = 0u; i < s_capacity; ++i) {
        CombusRxState& st = s_rxStates[i];

        // --- 2. Per-link guard check ---
        if (!st.nodeCom) { continue; }

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

        // --- 5. Contract timers (chantier 1.2) ---
        CombusHandshakeContext* ctx = st.handshakeCtx;
        if (!ctx || !ctx->contractValidated) { continue; }

        const uint32_t now = millis();
        if (ctx->dataSinceStable && st.linkTimeoutMs != 0u && (now - st.lastRxMs) > st.linkTimeoutMs) {
            combus_handshake_internal::dropContract(ctx, "silence");
        } else if (ctx->bidirectional && (now - ctx->lastAckMs) > kCombusHandshakeAckTimeoutMs) {
            combus_handshake_internal::dropContract(ctx, "no ACK, the peer does not receive this end");
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
    const bool contractOk = !st.handshakeCtx || st.handshakeCtx->contractValidated;   // chantier 1.2
    return contractOk && st.everReceived && (combus_rx_age_ms(linkIdx) < timeoutMs);
}



/** @brief Set the silence after which a receiving link counts as lost. */
void combus_rx_set_link_timeout( uint8_t linkIdx, uint32_t timeoutMs )
{
    if (!s_rxStates || linkIdx >= s_capacity) { return; }
    s_rxStates[linkIdx].linkTimeoutMs = timeoutMs;
}



/** @brief OR of one digital channel over the values received from the live links (both_or merge). */
bool combus_rx_peer_or( uint8_t ch, uint8_t excludeLink )
{
    if (!s_rxStates) { return false; }

    for (uint8_t i = 0u; i < s_capacity; ++i) {
        const CombusRxState& st = s_rxStates[i];

            // --- Only live links whose last frame carried the channel ---
        if (i == excludeLink || !st.nodeCom || !st.snap.digital || st.linkTimeoutMs == 0u) { continue; }
        if (!combus_rx_is_alive(i, st.linkTimeoutMs)) { continue; }

            // --- Wire position of the channel: its ID, or through the table of a
            //     link between nodes (3.1b), where it must be received ---
        uint8_t pos = ch;
        const ComPeerLink* peer = st.handshakeCtx ? st.handshakeCtx->peer : nullptr;
        if (peer) {
            pos = peer->nDigital;
            for (uint8_t p = 0u; p < peer->nDigital; ++p) {
                if (peer->digital[p] == ch && peer->digitalRx[p]) { pos = p; break; }
            }
        }
        if (pos >= st.snap.header.cfg.nDigital) { continue; }

        if (st.snap.digital[pos]) { return true; }
    }
    return false;
}

// EOF combus_rx.cpp