/*!****************************************************************************
 * @file  combus_link_defs.h
 * @brief Definitions of a ComBus link: what it carries, how, and on which port.
 *
 * @details A link is declared, not programmed:
 *   - ComPort (node_com.h): the hardware of a board;
 *   - ComDev: a logical link, shared by the boards or nodes it connects;
 *   - ComDevMap: the wiring of one end, a ComDev on a ComPort.
 *
 *   These are purely descriptive and live in Flash.  combus_links.h turns them into
 *   a ComBusLink at init.
 *******************************************************************************///
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <optional>

#include <core/system/hw/node_com.h>
#include <core/system/combus/combus_defs.h>                           // ChanLayer
#include <core/system/combus/protocol/handshake/combus_handshake.h>   // CombusHandshakeContext



// =============================================================================
// 1. LINK DESCRIPTION
// =============================================================================

/**
 * @brief Where a link goes: between nodes or between the boards of one node.
 *
 * @details Selects the channel set carried by the link (see linkScopeLayer()), and
 *   where its loss is reported.
 */

enum class LinkScope : uint8_t {
	INTER_NODE,     ///< between nodes (remote, trailer, ...): carries the BASE channels
	INTRA_NODE      ///< between the boards of one node: carries the SYSTEM channels
};


/**
 * @brief Whether a link carries data one way or both ways.
 *
 * @details No direction is given here: the direction of a unidirectional link is the
 *   role of each end (LinkEnd), set by the ComDevMap of each board.
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
 * @details ON_COUPLING: the link exists only while two elements are coupled, as told
 *   by the trigger of its CouplingDev.
 */

enum class LinkActivation : uint8_t {
	ALWAYS,
	ON_COUPLING
};


/**
 * @brief Protocol parameters of a link, agreed by both ends.
 */

struct ComProtocolCfg {
	uint32_t rate;                    ///< line rate (baud for a UART), identical at both ends
	uint32_t txHz;                    ///< transmitting end: rate of the data frames, 1 to 1000 Hz
	uint32_t linkTimeoutMs;           ///< receiving end: silence after which the link is lost (ms)
	uint32_t startupGraceMs = 3000u;  ///< receiving end, between boards: wait for a first frame before the loss (ms)
};


enum class DigitalComBusID : uint8_t;   // generated (combus_ids.h)


/**
 * @brief Identity of a coupled element: its type and its functions.
 *
 * @details Anchor for the identification (RFID or other, not implemented): read by
 *   CouplingDev::readId at the coupling, else CouplingDev::defaultId (the simple mode).
 *   No channel carries it yet.
 */

struct CouplingId {
	uint16_t type     = 0u;   ///< logical type of the coupled element (0 = unknown, simple mode)
	uint16_t features = 0u;   ///< mask of its functions
};


/**
 * @brief The coupling device of a link ON_COUPLING, or of a passive coupler.
 *
 * @details What says "coupled", and where the loss of the coupled link is reported.
 *   The contract of the link is its coupler (ComDev::peer).  See coupling/README.md
 *   for the whole sequence:
 *   - while the trigger is false, the link is off: nothing sent, nothing received, no
 *     handshake, no fault;
 *   - once true for `engageMs`, the two ends pair; paired, the link starts with the
 *     startup grace of its protocol;
 *   - lost after that grace, or not paired, the link reports it through `fail` only,
 *     never through NODE_LINK_FAIL;
 *   - it is detached once the trigger stays false for `releaseMs`.
 *
 *   A passive coupler (a tool, no link) has no ComDev: the machine registers it with
 *   combus_couplings_watch().  Its trigger is its detector, coupled once settled, with
 *   no pairing.
 *
 *   The chain of the coupler runs before the trigger is read: on the socket side, it
 *   latches the coupling button into the command that serves as the trigger.
 *
 *   `coupled` is written by the core.  For an active coupler: COUPLED, and the detector
 *   settled when there is one.  For a passive coupler: the detector settled.
 *
 *   The first six members come first, in this order: the tables are written in
 *   positional form, `{ name, trigger, fail, ageToleranceMs, engageMs, releaseMs }`.
 */

struct CouplingDev {
		// Identity and contributor
	const char*                    name;      ///< type of coupling, for the logs (ex. "fifth_wheel")
	DigitalComBusID                trigger;   ///< true = coupled (hardware or software, computed by the machine)
	std::optional<DigitalComBusID> fail;      ///< contributor of the coupled link (true = fault); absent = none

		// Timings
	uint16_t ageToleranceMs = 200u;   ///< the two ages of the hitch agree within this (depends on the actuator)
	uint16_t engageMs       = 50u;    ///< the trigger stays true this long before the pairing starts
	uint16_t releaseMs      = 500u;   ///< the trigger stays false this long before the link is detached

		// Chain of the coupler for this end, which writes the trigger; nullptr = none
	void (*chain)(ComBus& bus) = nullptr;

		// Channels of the state; absent = none
	std::optional<DigitalComBusID> coupled  = std::nullopt;   ///< coupled and settled, written by the core
	std::optional<DigitalComBusID> detector = std::nullopt;   ///< active coupler: presence detector, ANDed into `coupled`

		// Identity of the coupled element
	CouplingId defaultId                 = {};        ///< when none is read (simple mode)
	bool     (*readId)(CouplingId& out)  = nullptr;   ///< future reader (RFID...): true and `out` when read
};


/**
 * @brief A logical link of a node, or between nodes.
 *
 * @details Names no port and lists no channel: the channel set follows the scope, or
 *   the table of the link.  The loss of a receiving link is reported according to the
 *   scope too: BOARD_LINK_FAIL between boards, NODE_LINK_FAIL to another node, or the
 *   contributor of its CouplingDev for a coupled link.  The failsafe chain that reads
 *   the contributor decides the severity.
 */

struct ComDev {
	const char*           name;           ///< tag for logs and debug output
	LinkScope             scope;
	LinkType              type;
	const ComProtocolCfg* protocol;
	LinkActivation        activation;
	const CouplingDev*    coupling;       ///< required when activation == ON_COUPLING, else nullptr
	const ComPeerLink*    peer = nullptr; ///< INTER_NODE only: table of the link (combus_peer_links.h); nullptr = BASE view
};


/**
 * @brief The wiring of a ComDev on one board: which port, and which end.
 */

struct ComDevMap {
	const ComDev*  dev;
	const ComPort* port;
	LinkEnd        end;                   ///< BIDIRECTIONAL requires TX_RX
};



// =============================================================================
// 2. RUNNING LINK
// =============================================================================

/**
 * @brief All that is needed to start one link over an open transport.
 *
 * @details Filled by combus_link_build() from a ComDevMap, consumed by
 *   combus_link_start().  Only BASE and SYSTEM are valid layers: a BOARD channel never
 *   travels.  Each link owns its transport handle, its handshake context and its RX
 *   buffers.
 */

struct ComBusLink {
		// Identity of the link
	const char*        name  = "combus";               ///< tag for logs and debug output
	ChanLayer          layer = ChanLayer::UNDEFINED;   ///< BASE or SYSTEM: the channels carried, and the MD5
	const ComPeerLink* peer  = nullptr;                ///< link between nodes: table and MD5; nullptr = view of the layer

		// Transport and contract, shared by TX and RX of this link
	NodeCom*                com          = nullptr;
	CombusHandshakeContext* handshakeCtx = nullptr;

		// Transmission; txCfg absent = this end does not transmit
	std::optional<ComBusFrameCfg> txCfg = std::nullopt;   ///< capacity on transmission
	uint32_t                      txHz  = 0u;             ///< rate of the data frames

		// Reception; rxCfg absent = this end does not receive.  Buffers owned by the caller
	std::optional<ComBusFrameCfg> rxCfg      = std::nullopt;   ///< capacity on reception
	uint16_t*                     analogBuf  = nullptr;
	bool*                         digitalBuf = nullptr;
	ComBus*                       target     = nullptr;        ///< receives the accepted frames; nullptr = none (tests)
};

// EOF combus_link_defs.h
