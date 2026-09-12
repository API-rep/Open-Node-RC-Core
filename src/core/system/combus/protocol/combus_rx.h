/******************************************************************************
 * @file combus_rx.h
 * @brief ComBus receiver module
 *
 * @details Receives binary ComBus frames from any NodeCom* transport interface
 *   provided at init time. Validates SOF, length, and CRC-8/MAXIM, then
 *   exposes the latest valid snapshot via `combus_rx_snapshot()`.
 *
 *   The caller provides pre-allocated analog and digital output buffers at init
 *   time. The module writes decoded values directly into them on each valid frame.
 *   Incoming bytes are accumulated in an internal ring buffer.
 *   A SOF-scan re-synchronizes framing on corruption or line garbage.
 *
 * Typical integration (via combus_protocol_init_all — preferred):
 * @code
 *   // Caller-owned backing storage:
 *   static uint16_t analog[N_ANALOG];
 *   static bool     digital[N_DIGITAL];
 *
 *   // In setup():
 *   NodeCom* com = uart_com_init(&Serial2, BAUD, RX_PIN, TX_PIN, "sound_rx");
 *   constexpr ComBusFrameCfg cfg = { N_ANALOG, N_DIGITAL };
 *
 *   // Caller-owned per-link state pool (size = link count):
 *   static CombusRxState rxStates[1];
 *   combus_rx_register_pool(rxStates, 1);
 *
 *   // Caller-owned link descriptor (single source of truth):
 *   ComBusLink link{};
 *   link.com        = com;
 *   link.rxCfg      = cfg;
 *   link.analogBuf  = analog;
 *   link.digitalBuf = digital;
 *   link.layer      = ChanLayer::LOCAL;
 *   combus_protocol_init(0, &link);
 *
 *   // In loop():
 *   combus_rx_update();
 *   const ComBusFrame* snap = combus_rx_snapshot(0);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/hw/node_com.h>
#include <core/system/combus/combus_defs.h>
#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/combus/protocol/frame/combus_handshake.h>   // CombusHandshakeContext (full def needed for CombusRxState)


// =============================================================================
// 1. PROTOCOL CONSTANT  (single source of truth for the ring buffer size)
// =============================================================================

/**
 * @brief Ring buffer capacity for one ComBus RX link.
 *
 * @details Fixed protocol constraint, not a link count.  The protocol
 *   length field is a `uint8_t`, so no single frame can ever exceed
 *   UINT8_MAX.  This constant is the single source of truth for the
 *   per-link ring buffer size — used both as the array dimension in
 *   `CombusRxState::rxBuf` and as the modulo bound in the ring-buffer
 *   helpers in `combus_rx.cpp`.
 */
static constexpr uint8_t CombusRxBufSize = UINT8_MAX;  ///< max encodable frame size — protocol length field is uint8_t



// =============================================================================
// 2. PER-LINK RX STATE  (storage layout — allocated and owned by the caller)
// =============================================================================

/**
 * @brief One ComBus RX link state slot.
 *
 * @details Exposed (not opaque) so the caller (board) can declare its
 *   static array directly, without an internal core header.  Core never
 *   allocates this — only writes into slots of a buffer handed to it via
 *   combus_rx_register_pool().
 *
 *   Filled once by `combus_rx_init()` and updated every cycle by
 *   `combus_rx_update()`.  Holds the transport interface, the static
 *   frame layout, the per-link ring buffer (raw byte accumulator between
 *   the transport ISR and the frame decoder), the decoded snapshot with
 *   its validity flags, and the per-link handshake context.
 *
 *   The analog and digital pointers inside `snap` are wired to the
 *   caller-provided buffers at init time and never reallocated.
 *
 *   Lifetime: static — valid for the entire program run after init.
 */
struct CombusRxState {
    NodeCom*                nodeCom          = nullptr; ///< active transport interface
    ComBusFrameCfg          frameCfg         = {};      ///< static layout descriptor (nAnalog, nDigital)

    // Per-link ring buffer — raw byte accumulator between the transport
    // ISR and the frame decoder.  Sized to CombusRxBufSize (UINT8_MAX)
    // because the protocol length field is uint8_t.
    uint8_t                 rxBuf[CombusRxBufSize] = {};
    uint8_t                 rxHead           = 0u;      ///< write index (ISR side)
    uint8_t                 rxCount          = 0u;      ///< number of valid bytes in the ring buffer

    // Decoded snapshot — updated on each valid frame, read by the
    // application via combus_rx_snapshot().  The analog/digital pointers
    // are wired to caller-provided buffers at init time.
    ComBusFrame             snap             = {};
    bool                    snapValid        = false;   ///< true if snap holds a freshly-decoded frame
    uint32_t                lastRxMs         = 0u;      ///< timestamp of last successfully decoded frame (ms)
    bool                    everReceived     = false;   ///< true once at least one valid frame has been decoded

    // P3 — per-link handshake context.  Points to the same context as
    // CombusTxState::handshakeCtx (shared between TX and RX of the same
    // link).  Wired by combus_protocol_init() — see combus_protocol.cpp.
    // Multiple independent ComBus interfaces may coexist; each link has
    // its own context.  See CombusHandshakeContext in combus_handshake.h.
    CombusHandshakeContext* handshakeCtx     = nullptr;

    // RL5 — optional apply target.  When non-null, combus_rx_update()
    // calls combus_frame_apply() automatically after each successful
    // decode, writing the decoded channels into this ComBus instance.
    // When null (default), the caller is responsible for calling
    // combus_frame_apply() manually (legacy behaviour, used by tests).
    ComBus*                target            = nullptr;
};



// =============================================================================
// 3. POOL REGISTRATION  (call ONCE at boot, before any combus_rx_init)
// =============================================================================

/**
 * @brief Register the ComBus RX state pool storage.
 *
 * @details Core owns ZERO static storage for the per-link RX state.  The
 *   caller (machine, sound node, or any future integrator) allocates a
 *   static CombusRxState[] array sized to its own real needs and hands
 *   it to core exactly once via combus_rx_register_pool(), before the
 *   first combus_rx_init() call.  This pool is shared by every ComBus
 *   link in the program — its capacity is fixed for the program's
 *   lifetime.
 *
 *   A second call is rejected (logged, ignored) — the pool is meant to
 *   be wired once at boot, by whichever init sequence runs first.
 *
 * @param buffer    Statically-allocated CombusRxState array (caller-owned,
 *                  must outlive the program — no heap, no local/temporary
 *                  storage).
 * @param capacity  Number of usable slots in @p buffer.
 */
void combus_rx_register_pool( CombusRxState* buffer, uint8_t capacity );



// =============================================================================
// 4. PER-LINK INIT
// =============================================================================

/**
 * @brief Initialize the ComBus receiver for one link.
 *
 * @details Writes into the per-link state slot registered by
 *   combus_rx_register_pool().  linkIdx must be < the capacity passed
 *   to combus_rx_register_pool(); out-of-range indices are silently
 *   rejected.
 *
 * @param linkIdx     Index of this link in the per-link state array.
 * @param nodeCom     Claimed transport interface (from *_com_init).
 * @param frameCfg    ComBus layout descriptor (nAnalog, nDigital).
 * @param analogBuf   Caller-owned analog output buffer (size = frameCfg.nAnalog).
 * @param digitalBuf  Caller-owned digital output buffer (size = frameCfg.nDigital).
 * @param target      RL5 — optional ComBus instance to auto-apply decoded
 *                    frames onto.  When non-null, combus_rx_update() calls
 *                    combus_frame_apply() after each successful decode.
 *                    When null (default), no auto-apply is performed —
 *                    the caller must call combus_frame_apply() manually
 *                    (legacy behaviour, used by tests).
 */
void combus_rx_init( uint8_t            linkIdx,
                     NodeCom*           nodeCom,
                     ComBusFrameCfg     frameCfg,
                     uint16_t*          analogBuf,
                     bool*              digitalBuf,
                     ComBus*            target   = nullptr );



// =============================================================================
// 5. RECEIVE UPDATE
// =============================================================================

/**
 * @brief Drain the transport and decode any pending frame.
 *
 * @details Non-blocking.  Drains all available bytes from the transport
 *   into the per-link ring buffer, then attempts to decode one frame.
 *   Updates the internal snapshot on each valid frame.  Iterates over
 *   every link registered by combus_rx_register_pool().
 */
void combus_rx_update();



// =============================================================================
// 6. SNAPSHOT ACCESS
// =============================================================================

/**
 * @brief Get the latest decoded frame for one link.
 *
 * @details Returns a pointer to the per-link snapshot.  The pointer is
 *   valid for the entire program run; the contents are updated on each
 *   valid frame by combus_rx_update().  The caller must check the
 *   `valid` flag before reading analog/digital values.
 *
 * @param linkIdx  Index of this link in the per-link state array.
 * @return         Pointer to the per-link snapshot, or nullptr if
 *                 linkIdx is out of range.
 */
const ComBusFrame* combus_rx_snapshot( uint8_t linkIdx );



// =============================================================================
// 7. PER-LINK HANDSHAKE CONTEXT WIRING (P3)
// =============================================================================

/**
 * @brief Wire the per-link handshake context to the RX module.
 *
 * @details P3 — must be called BEFORE `combus_rx_init()` so the
 *   contract-validated flag is shared with the TX module of the same
 *   link.  Typically called from `combus_protocol_init()` with the
 *   same context as `combus_tx_set_handshake_ctx()`.
 *
 *   Multiple independent ComBus interfaces may coexist; each link has
 *   its own context.  See CombusHandshakeContext in combus_handshake.h.
 *
 * @param linkIdx  Index of this link in the per-link state array.
 * @param ctx      Per-link handshake context (may be null to disable sharing).
 */
void combus_rx_set_handshake_ctx( uint8_t                linkIdx,
                                  CombusHandshakeContext* ctx );

// EOF combus_rx.h