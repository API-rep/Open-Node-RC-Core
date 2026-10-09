/******************************************************************************
 * @file combus_tx.h
 * @brief ComBus transmitter module
 *
 * @details Serializes the live ComBus into a binary frame and sends it via
 * any NodeCom* physical transport interface (UART, ESP-Now, …) provided at
 * init time. Timer-gated and non-blocking: the update function does nothing
 * if the transmit interval has not elapsed since the last frame.
 *
 * Typical integration (via combus_protocol_init_all — preferred):
 * @code
 *   // In init:
 *   NodeCom* com = uart_com_init(&Serial2, BAUD, TX_PIN, RX_PIN, "combus");
 *   constexpr ComBusFrameCfg cfg = { N_ANALOG, N_DIGITAL };
 *
 *   // Caller-owned per-link state pool (size = link count):
 *   static CombusTxState txStates[1];
 *   combus_tx_register_pool(txStates, 1);
 *
 *   // Caller-owned link descriptor (single source of truth):
 *   ComBusLink link{};
 *   link.com   = com;
 *   link.txCfg = cfg;
 *   link.txHz  = TX_HZ;
 *   link.layer = ChanLayer::REMOTE;   // LY2 — selects wireEnd from Remote view
 *   combus_protocol_init(0, &link);
 *
 *   // In loop:
 *   combus_tx_update(&comBus);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/node_com.h>
#include <core/system/combus/combus_defs.h>
#include <core/system/combus/protocol/frame/combus_handshake.h>   // CombusHandshakeContext (full def needed for CombusTxState)


// =============================================================================
// 1. PER-LINK TX STATE  (storage layout — allocated and owned by the caller)
// =============================================================================

/**
 * @brief One ComBus TX link state slot.
 *
 * @details Exposed (not opaque) so the caller (board) can declare its
 *   static array directly, without an internal core header.  Core never
 *   allocates this — only writes into slots of a buffer handed to it via
 *   combus_tx_register_pool().
 *
 *   Filled once by `combus_tx_init()` and read every cycle by
 *   `combus_tx_update()`.  Holds the transport interface, the static
 *   frame layout, the rolling sequence counter, the timer state needed
 *   for the transmit rate gate, and the per-link handshake context.
 *
 *   Lifetime: static — valid for the entire program run after init.
 */
struct CombusTxState {
    NodeCom*               nodeCom  = nullptr;  ///< active transport interface
    ComBusFrameCfg         frameCfg = {};       ///< static layout descriptor (nAnalog, nDigital)

    uint8_t                seq      = 1u;       ///< rolling frame sequence counter (1..255). Value 0 is RESERVED for future handshake frames.
    uint32_t               lastTxMs = 0u;       ///< timestamp of last transmitted frame (ms)
    uint32_t               periodMs = 0u;       ///< transmit period derived from txHz (0 = uninit)

    // LY2 — wire-end counters resolved at TX init from link->layer.
    // The codec iterates only over comBus.analogBus[0..analogWireEnd)
    // and comBus.digitalBus[0..digitalWireEnd) — this is the ONLY
    // behavioural change vs. the pre-LY2 codec.  See combus_tx_init()
    // for the resolution rules.
    uint8_t                analogWireEnd  = 0u; ///< number of analog channels to encode (layer's CH_COUNT)
    uint8_t                digitalWireEnd = 0u; ///< number of digital channels to encode (layer's CH_COUNT)

    // P3 — per-link handshake context.  Points to the same context as
    // CombusRxState::handshakeCtx (shared between TX and RX of the same
    // link).  Wired by combus_protocol_init() — see combus_protocol.cpp.
    // Multiple independent ComBus interfaces may coexist; each link has
    // its own context.  See CombusHandshakeContext in combus_handshake.h.
    CombusHandshakeContext* handshakeCtx = nullptr;
};



// =============================================================================
// 2. POOL REGISTRATION  (call ONCE at boot, before any combus_tx_init)
// =============================================================================

/**
 * @brief Register the ComBus TX state pool storage.
 *
 * @details Core owns ZERO static storage for the per-link TX state.  The
 *   caller (machine, sound node, or any future integrator) allocates a
 *   static CombusTxState[] array sized to its own real needs and hands
 *   it to core exactly once via combus_tx_register_pool(), before the
 *   first combus_tx_init() call.  This pool is shared by every ComBus
 *   link in the program — its capacity is fixed for the program's
 *   lifetime.
 *
 *   A second call is rejected (logged, ignored) — the pool is meant to
 *   be wired once at boot, by whichever init sequence runs first.
 *
 * @param buffer    Statically-allocated CombusTxState array (caller-owned,
 *                  must outlive the program — no heap, no local/temporary
 *                  storage).
 * @param capacity  Number of usable slots in @p buffer.
 */
void combus_tx_register_pool( CombusTxState* buffer, uint8_t capacity );



// =============================================================================
// 3. PER-LINK INIT
// =============================================================================

/**
 * @brief Initialize the ComBus transmitter for one link.
 *
 * @details Writes into the per-link state slot registered by
 *   combus_tx_register_pool().  linkIdx must be < the capacity passed
 *   to combus_tx_register_pool(); out-of-range indices are silently
 *   rejected.
 *
 * LY2 — the wire-end counters are resolved here from the link's
 * `ChanLayer` (REMOTE → CH_COUNT of the Remote view, LOCAL → CH_COUNT
 * of the Local view, FULL → CH_COUNT of the Full view).  The codec
 * iterates only over comBus.analogBus[0..analogWireEnd) and
 * comBus.digitalBus[0..digitalWireEnd) — this is the ONLY behavioural
 * change vs. the pre-LY2 codec.
 *
 * @param linkIdx   Index of this link in the per-link state array.
 * @param nodeCom   Claimed transport interface (from *_com_init).
 * @param frameCfg  ComBus layout descriptor (nAnalog, nDigital).
 * @param txHz      Frame transmit rate in Hz.
 * @param layer     ChanLayer of this link — selects the wire-end counters.
 */
void combus_tx_init( uint8_t         linkIdx,
                     NodeCom*        nodeCom,
                     ComBusFrameCfg  frameCfg,
                     uint32_t        txHz,
                     ChanLayer       layer );



// =============================================================================
// 4. TRANSMIT UPDATE
// =============================================================================

/**
 * @brief Encode and transmit one ComBus frame if the period has elapsed.
 *
 * @details Timer-gated and non-blocking. Does nothing if the transmit interval
 *   has not elapsed since the last frame.  Iterates over every link
 *   registered by combus_tx_register_pool() — each link has its own
 *   timer gate, sequence counter, and handshake burst state.
 *
 * @param bus         Live ComBus to encode.
 * @param failSafe    Set true to flag the frame as failsafe-active.
 */
void combus_tx_update( const ComBus* bus );



// =============================================================================
// 5. PER-LINK HANDSHAKE CONTEXT WIRING (P3)
// =============================================================================

/**
 * @brief Wire the per-link handshake context to the TX module.
 *
 * @details P3 — must be called BEFORE `combus_tx_init()` so the burst
 *   is armed on init.  Typically called from `combus_protocol_init()`
 *   with the same context as `combus_rx_init()` (shared between TX
 *   and RX of the same link).
 *
 *   Multiple independent ComBus interfaces may coexist; each link has
 *   its own context.  See CombusHandshakeContext in combus_handshake.h.
 *
 * @param linkIdx  Index of this link in the per-link state array.
 * @param ctx      Per-link handshake context (may be null to disable burst).
 */
void combus_tx_set_handshake_ctx( uint8_t                linkIdx,
                                  CombusHandshakeContext* ctx );

// EOF combus_tx.h