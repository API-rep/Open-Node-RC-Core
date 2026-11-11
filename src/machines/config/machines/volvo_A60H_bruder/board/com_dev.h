/*!****************************************************************************
 * @file    com_dev.h
 * @brief   Volvo A60H Bruder — communication links of the node (ComDev).
 *
 * @details A ComDev is a logical link of the node, defined once here and shared
 *   by every board: each board wires it on one of its own ports with a
 *   ComDevMap, so swapping one board for another changes the wiring but not the
 *   link.
 *
 *   The links of this file are intra-node: they carry the SYSTEM channel set,
 *   which contains the BASE channels.  Their protocol parameters (line rate,
 *   transmit rate) are agreed by every board that wires them.
 *
 *   Each board wires them in its table of links (comLinks[] of its envCfg.h).
 *******************************************************************************
 */
#pragma once

#include <core/system/combus/protocol/link/combus_link_defs.h>   // ComDev, ComProtocolCfg


// =============================================================================
// 1. EXTENSION LINK
// =============================================================================

	/// Protocol of the extension link: 115200 baud, 50 frames per second, lost after 250 ms of silence,
	/// 3 s for a first frame before the link counts as lost: intra-node, its loss feeds BOARD_LINK_FAIL.
inline constexpr ComProtocolCfg kComProtoExt = { 115200u, 50u, 250u, 3000u };


	/// One-way extension link: the main board wires it as TX, the extension board as RX.
inline constexpr ComDev kComDevExtUni = {
	"combus", LinkScope::INTRA_NODE, LinkType::UNIDIRECTIONAL,
	&kComProtoExt, LinkActivation::ALWAYS, nullptr
};


	/// Two-way extension link: both boards wire it as TX_RX.
inline constexpr ComDev kComDevExtBi = {
	"combus", LinkScope::INTRA_NODE, LinkType::BIDIRECTIONAL,
	&kComProtoExt, LinkActivation::ALWAYS, nullptr
};

// EOF com_dev.h
