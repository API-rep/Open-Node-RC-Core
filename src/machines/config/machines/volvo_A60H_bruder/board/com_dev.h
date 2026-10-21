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
 *   Until the boards declare their own ComDevMap table, the COMBUS_UART_TX=N and
 *   COMBUS_UART=N build flags choose which of the two extension links is wired
 *   (see machines/init/com/combus_uart_init.cpp).
 *******************************************************************************
 */
#pragma once

#include <core/system/combus/protocol/combus_protocol.h>   // ComDev, ComProtocolCfg


// =============================================================================
// 1. EXTENSION LINK
// =============================================================================

	/// Protocol of the extension link: 115200 baud, 50 frames per second.
inline constexpr ComProtocolCfg kComProtoExt = { 115200u, 50u };


	/// One-way extension link (COMBUS_UART_TX=N): the board wires it as TX.
inline constexpr ComDev kComDevExtUni = {
	"combus", LinkScope::INTRA_NODE, LinkType::UNIDIRECTIONAL,
	&kComProtoExt, LinkActivation::ALWAYS, nullptr
};


	/// Two-way extension link (COMBUS_UART=N): the board wires it as TX_RX.
inline constexpr ComDev kComDevExtBi = {
	"combus", LinkScope::INTRA_NODE, LinkType::BIDIRECTIONAL,
	&kComProtoExt, LinkActivation::ALWAYS, nullptr
};

// EOF com_dev.h
