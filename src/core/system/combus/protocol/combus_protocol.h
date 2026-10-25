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
 * ComBusLink descriptor (see section 2 below).  The function inspects
 * link->txCfg and link->rxCfg to decide which protocol layers to bring up:
 *   - txCfg present  → combus_tx_init() is called
 *   - rxCfg present  → combus_rx_init() is called
 *   - both absent    → no-op (link is fully inactive)
 *
 * The caller retains ownership of analogBuf and digitalBuf — they must have
 * static storage duration because the RX module holds a live pointer to them.
 * Pass nullptr for the inactive direction's buffers.
 *
 * Links are normally described with a ComDev (the logical link) and a
 * ComDevMap (its wiring on a board's ComPort) and brought up by
 * combus_links_init() (combus_links.h), which opens the ports, builds the
 * ComBusLink descriptors and delegates here. Env-level callers invoke the
 * 0-param combus_uart_init() defined in init/com/combus_uart_init.h.
 *
 * Typical TX-only caller (manual setup, without ComDev):
 * @code
 *   // Resolve UART channel + pins from the board's ComPort, then
 *   // open the port directly via uart_com_init() (no single-link helper).
 *   constexpr int uartCh    = COMBUS_UART_TX;   // or COMBUS_UART
 *   const     int uartTxPin = port.uart.txPin;  // port : ComPort of UART uartCh
 *   constexpr int uartRxPin = -1;
 *   NodeCom* com = uart_com_init(uart_serial_for(uartCh), kComProtoExt.rate,
 *                                uartTxPin, uartRxPin, "combus", &pinReg);
 *
 *   constexpr ComBusFrameCfg txCfg = { N_ANALOG, N_DIGITAL };
 *   ComBusLink link = {};
 *   link.com   = com;
 *   link.txCfg = txCfg;
 *   link.txHz  = kComProtoExt.txHz;
 *   link.layer = ChanLayer::BASE;   // LY2 — selects wireEnd from Base view
 *   combus_protocol_init(&link);
 * @endcode
 *
 * Typical RX-only caller (manual setup, without ComDev):
 * @code
 *   // Resolve UART channel + pins from the board's ComPort, then
 *   // open the port directly via uart_com_init() (no single-link helper).
 *   constexpr int uartCh    = COMBUS_UART_RX;   // or COMBUS_UART
 *   constexpr int uartTxPin = -1;
 *   const     int uartRxPin = port.uart.rxPin;  // port : ComPort of UART uartCh
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
 *   link.layer      = ChanLayer::BASE;
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
// 1. LINK DESCRIPTION  (ComDev / ComDevMap)
// =============================================================================
//
//   ComPort    (node_com.h)  the hardware of a board.
//   ComDev                   a logical link: what it carries and how.
//   ComDevMap                the wiring of one board: a ComDev on a ComPort.
//
//   A ComDev is shared by the boards it links (and, for inter-node links, by
//   the nodes), each side wiring it on its own port with its own ComDevMap.
//   All three are purely descriptive and Flash-resident.  combus_links.h turns
//   them into ComBusLink descriptors (section 2) at init.
//

/**
 * @brief Where a link goes: between nodes or between the boards of one node.
 *
 * @details Selects the channel set carried by the link (see linkScopeLayer()).
 */

enum class LinkScope : uint8_t {
    INTER_NODE,     ///< between nodes (remote, trailer, ...) — carries the BASE channels
    INTRA_NODE      ///< between the boards of one node — carries the SYSTEM channels
};


/**
 * @brief Whether a link carries data one way or both ways.
 *
 * @details No direction is given here: the direction of a unidirectional
 *   link is the role of each end (LinkEnd), set by each board's ComDevMap.
 */

enum class LinkType : uint8_t {
    UNIDIRECTIONAL,
    BIDIRECTIONAL
};


/**
 * @brief Role of one end of a link, as wired by one board.
 */

enum class LinkEnd : uint8_t {
    TX,             ///< this end only transmits
    RX,             ///< this end only receives
    TX_RX           ///< this end transmits and receives (required by BIDIRECTIONAL)
};


/**
 * @brief When a link is active.
 *
 * @details ON_COUPLING is reserved for the coupling of two elements (see
 *   ComCoupling); it is refused for now.
 */

enum class LinkActivation : uint8_t {
    ALWAYS,
    ON_COUPLING
};


/**
 * @brief Protocol parameters of a link, agreed by both ends.
 */

struct ComProtocolCfg {
    uint32_t rate;           ///< line rate (baud for a UART), identical at both ends
    uint32_t txHz;           ///< frame transmit rate (Hz) — used where the end transmits
    uint32_t linkTimeoutMs;  ///< receiving end: silence after which the link is declared lost (ms)
    uint32_t startupGraceMs = 3000u;  ///< receiving end: wait for a first frame before a FAILSAFE loss (ms)
};


/**
 * @brief What the loss of a receiving link triggers on the board.
 *
 * @details BY_SCOPE resolves to FAILSAFE for an intra-node link and to IDLE for
 *   an inter-node link (see linkLossAction()). IDLE applies as soon as the link
 *   is not alive, never seen included; FAILSAFE waits for the startup grace
 *   before a link never seen counts as lost.
 */

enum class LinkLossAction : uint8_t {
    BY_SCOPE,       ///< default: FAILSAFE for INTRA_NODE, IDLE for INTER_NODE
    NONE,           ///< the loss is only logged
    IDLE,           ///< the loss drives COM_LINK_LOST (REMOTE_LINK_LOST -> runlevel IDLE)
    FAILSAFE        ///< the loss drives FAILSAFE_COM_LINK (FAILSAFE)
};


struct ComCoupling;     // trigger channel and expected value — defined with the coupling logic


/**
 * @brief A logical link of a node (or between nodes).
 *
 * @details Does not name a port and does not list channels: the channel set
 *   follows the scope.
 */

struct ComDev {
    const char*            name;        ///< tag for logs and debug output
    LinkScope              scope;
    LinkType               type;
    const ComProtocolCfg*  protocol;
    LinkActivation         activation;  ///< only ALWAYS is accepted for now
    const ComCoupling*     coupling;    ///< nullptr unless activation == ON_COUPLING
    LinkLossAction         onLoss = LinkLossAction::BY_SCOPE;   ///< reaction to the loss of the link
    const ComPeerLink*     peer   = nullptr;   ///< INTER_NODE only: the link with a peer type (combus_peer_links.h), nullptr = BASE view
};


/**
 * @brief The wiring of a ComDev on one board: which port, and which end.
 */

struct ComDevMap {
    const ComDev*   dev;
    const ComPort*  port;
    LinkEnd         end;                ///< BIDIRECTIONAL requires TX_RX
};


/**
 * @brief Channel set (ChanLayer) carried by a link of the given scope.
 *
 * @param scope  Scope of the link.
 * @return       ChanLayer::BASE for INTER_NODE, ChanLayer::SYSTEM for INTRA_NODE.
 */

constexpr ChanLayer linkScopeLayer( LinkScope scope )
{
    return (scope == LinkScope::INTER_NODE) ? ChanLayer::BASE : ChanLayer::SYSTEM;
}


/**
 * @brief Reaction to the loss of a link, BY_SCOPE resolved.
 *
 * @param dev  Link description.
 * @return     dev.onLoss, or FAILSAFE (INTRA_NODE) / IDLE (INTER_NODE) for BY_SCOPE.
 */

constexpr LinkLossAction linkLossAction( const ComDev& dev )
{
    if (dev.onLoss != LinkLossAction::BY_SCOPE) { return dev.onLoss; }
    return (dev.scope == LinkScope::INTRA_NODE) ? LinkLossAction::FAILSAFE : LinkLossAction::IDLE;
}



// =============================================================================
// 2. COMBUS LINK DESCRIPTOR
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
 * @note Only ChanLayer::BASE and ChanLayer::SYSTEM are valid here.
 *   ChanLayer::BOARD describes an intra-device loopback that is never
 *   transported over a physical link and must therefore never populate
 *   a ComBusLink.
 *
 * @note LY2 — `layer` selects the wire-end counters at TX init time
 *   (BASE → CH_COUNT of the Base view, SYSTEM → CH_COUNT of the
 *   System view, FULL → CH_COUNT of the Full view).  The codec iterates
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
    ChanLayer                      layer        = ChanLayer::UNDEFINED; ///< Propagation layer of this link — BASE or SYSTEM only (never BOARD). LY2 — also selects the wire-end counters at TX init.
    const char*                    name         = "combus";  ///< Descriptive tag for logs and debug output.
    ComBus*                        target       = nullptr;   ///< RL5 — optional ComBus instance to auto-apply decoded RX frames onto.  When non-null, combus_rx_update() calls combus_frame_apply() after each successful decode.  When null (default), no auto-apply is performed — caller must call combus_frame_apply() manually.
    const ComPeerLink*             peer         = nullptr;   ///< Link rework 3.1b — link between nodes: common frame, table and MD5 of the link.  nullptr = view of the layer.
};



// =============================================================================
// 3. PUBLIC API
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
 *   Failure modes:
 *     - link == nullptr or link->com == nullptr  → return immediately
 *     - link->layer == ChanLayer::UNDEFINED      → FATAL: logged via
 *                                                  sys_log_err, system halted
 *     - link->layer == ChanLayer::BOARD         → accepted: the handshake
 *                                                  uses the FULL view MD5
 *                                                  (reserved for a loopback)
 *
 * @param linkIdx  Index of this link in the per-link state array
 *                 (allocated by combus_protocol_init_all()).
 * @param link     Fully-populated ComBusLink descriptor (see section 2).
 *                 Must outlive the program — the protocol layers retain
 *                 pointers to its members.
 */
void combus_protocol_init( uint8_t linkIdx, ComBusLink* link );

// EOF combus_protocol.h