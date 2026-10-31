/*!****************************************************************************
 * @file  combus_peer_link_defs.h
 * @brief Definitions of a link between two nodes through a coupler (link rework 3.1b, 3.4a).
 *
 * @details Pure type definitions, filled by the builder (combus_peer_links.h,
 *   one ComPeerLink per coupler of the node, -D COMBUS_COUPLER_<NAME>).
 *******************************************************************************///
#pragma once

#include <stdint.h>
#include <stdbool.h>



/**
 * @brief A contract of a link: the MD5 of the stable set and of the next set.
 *
 * @details The next set adds the candidates (chantier 2.2b); without
 *   candidate, both MD5 are the same.
 */

struct ComLinkMd5 {
    const uint8_t* stable;   ///< MD5 without the candidates (16 bytes)
    const uint8_t* next;     ///< MD5 with the candidates (16 bytes)
};


/**
 * @brief One half of the common frame of a link (analog, or digital), and its table.
 *
 * @details The stable positions are a prefix of the frame: the candidates
 *   follow them.
 */

struct ComLinkTable {
    uint8_t        count;         ///< positions on the wire
    uint8_t        stableCount;   ///< positions without the candidates (a prefix)
    const uint8_t* ids;           ///< wire position -> channel ID of this node
    const bool*    rx;            ///< wire position -> this node receives it (true) or sends it
};


/**
 * @brief The common frame of a link between two nodes, and its table.
 *
 * @details The channel set of the link is the full coupler, in wire order
 *   (analog then digital, stable before candidate).  Each wire position maps
 *   to a channel of this node's ComBus, and says whether this node receives it
 *   on the link or sends it.
 *
 *   The MD5 carries the coupler and a role (vehicle or attachment): this node
 *   announces its role (`announced`) and expects the other one (`expected`),
 *   so two vehicles, or two attachments, linked by mistake refuse each other.
 *
 *   A link without this descriptor carries the prefix of its scope's view,
 *   wire position = channel ID (BASE or SYSTEM).
 */

struct ComPeerLink {
    const char*  coupler;     ///< coupler of the link (directory name)
    ComLinkMd5   announced;   ///< MD5 announced by this node (its role)
    ComLinkMd5   expected;    ///< MD5 expected from the other end (the other role)
    ComLinkTable analog;      ///< analog half of the frame (AnalogComBusID)
    ComLinkTable digital;     ///< digital half of the frame (DigitalComBusID)
};

// EOF combus_peer_link_defs.h
