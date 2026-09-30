/******************************************************************************
 * @file combus_protocol.h
 * @brief ComBus protocol layer — wire TX/RX protocol layers to a transport.
 *
 * @details Initialises the TX and/or RX ComBus protocol layers over any
 * NodeCom* transport that was already opened by the caller (e.g.
 * uart_com_init()).  Called after hw_init(), from the env-level com_init()
 * wrapper.
 *
 * The transport handle and its configuration are passed in as a single
 * ComBusLink descriptor (see section 1 below).  The function inspects
 * link->txCfg and link->rxCfg to decide which protocol layers to bring up:
 *   - txCfg present  → combus_tx_init() is called
 *   - rxCfg present  → combus_rx_init() is called
 *   - both absent    → no-op (link is fully inactive)
 *
 * The caller retains ownership of analogBuf and digitalBuf — they must have
 * static storage duration because the RX module holds a live pointer to them.
 * Pass nullptr for the inactive direction's buffers.
 *
 * Env-level callers invoke the 0-param combus_uart_init() defined in
 * init/com/combus_uart_init.h, which fills env-specific config and buffers
 * then delegates here.
 *
 * Typical TX-only caller (env wrapper):
 * @code
 *   // Resolve UART channel + pins from the active COMBUS_UART* flag, then
 *   // open the port directly via uart_com_init() (no single-link helper).
 *   constexpr int uartCh    = COMBUS_UART_TX;   // or COMBUS_UART
 *   const     int uartTxPin = uartPins[uartCh].tx;
 *   constexpr int uartRxPin = -1;
 *   NodeCom* com = uart_com_init(uart_serial_for(uartCh), ComBusUartBaud,
 *                                uartTxPin, uartRxPin, "combus", &pinReg);
 *
 *   constexpr ComBusFrameCfg txCfg = { N_ANALOG, N_DIGITAL };
 *   ComBusLink link = {};
 *   link.com   = com;
 *   link.txCfg = txCfg;
 *   link.txHz  = ComBusUartTxHz;
 *   link.layer = ChanLayer::REMOTE;   // LY2 — selects wireEnd from Remote view
 *   combus_protocol_init(&link);
 * @endcode
 *
 * Typical RX-only caller (env wrapper):
 * @code
 *   // Resolve UART channel + pins from the active COMBUS_UART* flag, then
 *   // open the port directly via uart_com_init() (no single-link helper).
 *   constexpr int uartCh    = COMBUS_UART_RX;   // or COMBUS_UART
 *   constexpr int uartTxPin = -1;
 *   const     int uartRxPin = uartPins[uartCh].rx;
 *   NodeCom* com = uart_com_init(uart_serial_for(uartCh), SOUND_UART_BAUD,
 *                                uartTxPin, uartRxPin, "combus", &pinReg);
 *
 *   static uint16_t analog[N_ANALOG];
 *   static bool     digital[N_DIGITAL];
 *   constexpr ComBusFrameCfg rxCfg = { N_ANALOG, N_DIGITAL };
 *   ComBusLink link = {};
 *   link.com        = com;
 *   link.rxCfg      = rxCfg;
 *   link.analogBuf  = analog;
 *   link.digitalBuf = digital;
 *   link.layer      = ChanLayer::REMOTE;
 *   combus_protocol_init(&link);
 * @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <optional>

#include <core/system/hw/node_com.h>
#include <core/system/combus/combus_defs.h>                       // ChanLayer
#include <core/system/combus/protocol/frame/combus_handshake.h>   // CombusHandshakeContext (P3)



// =============================================================================
// 1. COMBUS LINK DESCRIPTOR
// =============================================================================

/**
 * @brief Descriptive structure of one ComBus physical link.
 *
 * @details Aggregates every parameter needed to bring up a single ComBus
 *   link over any NodeCom* transport. A link is the unit of physical
 *   connectivity between two nodes (e.g. machine <-> sound node over
 *   UART2, or machine <-> remote over ESP-NOW). Multiple links may
 *   coexist in the same program; each owns its own transport handle,
 *   its own handshake context, and its own RX buffers.
 *
 *   The struct is purely descriptive at this stage — combus_protocol_init()
 *   still consumes its parameters positionally. A future revision will
 *   accept a ComBusLink (or an array of them) directly.
 *
 * @note Only ChanLayer::REMOTE and ChanLayer::LOCAL are valid here.
 *   ChanLayer::SYSTEM describes an intra-device loopback that is never
 *   transported over a physical link and must therefore never populate
 *   a ComBusLink.
 *
 * @note LY2 — `layer` selects the wire-end counters at TX init time
 *   (REMOTE → CH_COUNT of the Remote view, LOCAL → CH_COUNT of the
 *   Local view, FULL → CH_COUNT of the Full view).  The codec iterates
 *   only over comBus.analogBus[0..analogWireEnd) and
 *   comBus.digitalBus[0..digitalWireEnd) — this is the ONLY behavioural
 *   change vs. the pre-LY2 codec.
 */
struct ComBusLink {
    NodeCom*                       com          = nullptr;  ///< Transport handle, shared by TX and RX of this link.
    std::optional<ComBusFrameCfg>  txCfg        = std::nullopt; ///< TX frame layout — absent = TX inactive on this link.
    uint32_t                       txHz         = 0u;        ///< TX frame rate (Hz) — ignored when txCfg is absent.
    std::optional<ComBusFrameCfg>  rxCfg        = std::nullopt; ///< RX frame layout — absent = RX inactive on this link.
    uint16_t*                      analogBuf    = nullptr;   ///< Caller-owned RX analog buffer — ignored when rxCfg is absent.
    bool*                          digitalBuf   = nullptr;   ///< Caller-owned RX digital buffer — ignored when rxCfg is absent.
    CombusHandshakeContext*        handshakeCtx = nullptr;   ///< Per-link handshake state (P3) — shared between TX and RX of this link.
    ChanLayer                      layer        = ChanLayer::UNDEFINED; ///< Propagation layer of this link — REMOTE or LOCAL only (never SYSTEM). LY2 — also selects the wire-end counters at TX init.
    const char*                    name         = "combus";  ///< Descriptive tag for logs and debug output.
    ComBus*                        target       = nullptr;   ///< RL5 — optional ComBus instance to auto-apply decoded RX frames onto.  When non-null, combus_rx_update() calls combus_frame_apply() after each successful decode.  When null (default), no auto-apply is performed — caller must call combus_frame_apply() manually.
};



// =============================================================================
// 2. PUBLIC API
// =============================================================================

/**
 * @brief Initialise every ComBus link in a caller-owned array.
 *
 * @details Iterates over the caller-owned ComBusLink[] array and calls
 *   combus_protocol_init() for each entry.  This module does NOT
 *   allocate any state — the caller (board) is responsible for
 *   declaring its own static CombusTxState[] pool and registering it
 *   via combus_tx_register_pool() BEFORE calling this function.
 *
 *   The number of links is decided by the caller (typically
 *   sizeof(links)/sizeof(links[0]) on a caller-owned ComBusLink[]
 *   array).  This module never imposes a hard cap on the link count.
 *
 * @param links  Caller-owned array of ComBusLink descriptors.
 * @param count  Number of entries in @p links.
 */
void combus_protocol_init_all( ComBusLink* links, uint8_t count );



/**
 * @brief Initialise the ComBus protocol layers for one physical link.
 *
 * @details Inspects @p link to decide which protocol layers to bring up:
 *   - link->txCfg present  → combus_tx_init() is called with link->com,
 *                             *link->txCfg, link->txHz, link->layer
 *   - link->rxCfg present  → combus_rx_init() is called with link->com,
 *                             *link->rxCfg, link->analogBuf, link->digitalBuf
 *   - both absent          → no-op (link is fully inactive)
 *
 *   The per-link handshake context (link->handshakeCtx) is wired to both
 *   TX and RX BEFORE their respective init calls, so the burst is armed
 *   on combus_tx_init() and the contract-validated flag is shared between
 *   TX and RX of the same link (P3).
 *
 *   The transport itself (link->com) must have been opened by the caller
 *   (e.g. uart_com_init()) before calling this function.
 *
 *   The caller retains ownership of link->analogBuf and link->digitalBuf —
 *   they must have static storage duration because the RX module holds a
 *   live pointer to them.
 *
 *   Silent failure modes (logged via sys_log_err, no crash):
 *     - link == nullptr or link->com == nullptr  → return immediately
 *     - link->layer == ChanLayer::UNDEFINED      → return (uninitialised)
 *     - link->layer == ChanLayer::SYSTEM         → return (loopback, never
 *                                                   transported physically)
 *
 * @param linkIdx  Index of this link in the per-link state array
 *                 (allocated by combus_protocol_init_all()).
 * @param link     Fully-populated ComBusLink descriptor (see section 1).
 *                 Must outlive the program — the protocol layers retain
 *                 pointers to its members.
 */
void combus_protocol_init( uint8_t linkIdx, ComBusLink* link );

// EOF combus_protocol.h