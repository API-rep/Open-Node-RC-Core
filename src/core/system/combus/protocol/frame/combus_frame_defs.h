/*!****************************************************************************
 * @file  combus_frame_defs.h
 * @brief Definitions of the ComBus control frame.
 *
 * @details Pure type definitions, with no feature flag dependency.  The constants
 *   of each transport are in src/core/config/com/.
 *******************************************************************************///
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/protocol/combus_peer_link_defs.h>   // ComPeerLink


/**
 * @brief Static layout of a frame: its channel counts.
 *
 * @details Set once at init, never changed at runtime.  Shared by the codec and by
 *   the protocol modules (combus_tx, combus_rx).  On a remote, one instance per
 *   driven machine type.
 *
 * @warning This struct is the first member of CombusFrameHeader and maps directly
 *   to the first two wire bytes after the SOF.  Any change of its fields breaks the
 *   wire protocol, and requires the same change in combus_frame_encode() and in the
 *   memcpy-based decode.
 */

struct ComBusFrameCfg {
	uint8_t nAnalog;   ///< Number of analog channels in this layout.
	uint8_t nDigital;  ///< Number of digital channels in this layout.
};


/**
 * @brief Header of a frame: the wire layout, and part of the decoded frame.
 *
 * @details Transmitted right after the SOF, in this order: nAnalog, nDigital, seq,
 *   flags.  Mapping the first two bytes to ComBusFrameCfg allows a memcpy-based
 *   decode.
 *
 * @note `seq == 0` is reserved on every transport: see combus_frame.h.
 */

struct CombusFrameHeader {
	ComBusFrameCfg cfg;   ///< Layout of this frame: nAnalog, nDigital (wire offsets 0-1).
	uint8_t seq;          ///< Rolling frame counter, 1..255; 0 is reserved.
	uint8_t flags;        ///< Handshake and coupling frames: kind of the frame; control frames: 0.
};


/**
 * @brief Decoded ComBus frame: the header and the decoded payload.
 *
 * @details The effective sizes of `analog` and `digital` are the counts of the header.
 *   Both are indexed by wire position.
 */

struct ComBusFrame {
	CombusFrameHeader header;   ///< Wire header fields (offsets 1-4 of the frame).
	uint16_t*         analog;   ///< Caller-provided buffer (at least header.cfg.nAnalog entries)
	bool*             digital;  ///< Caller-provided buffer (at least header.cfg.nDigital entries)
};

// EOF combus_frame_defs.h
