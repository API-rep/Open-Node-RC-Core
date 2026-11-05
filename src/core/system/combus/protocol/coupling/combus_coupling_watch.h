/******************************************************************************
 * @file  combus_coupling_watch.h
 * @brief Follow-up of the couplings: from the state of a pairing to the state of a link.
 *
 * @details combus_pairing decides who is hitched to whom; this module acts on it: it
 *   turns a link on or off, and writes the contributors of the coupler.  It also steps
 *   the passive couplers, which have no link.  Called each cycle by combus_links_update().
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <core/system/combus/protocol/link/combus_links.h>   // ComLinkRuntime



// =============================================================================
// 1. LINK ON_COUPLING
// =============================================================================

/**
 * @brief Step the pairing of a coupled link, and follow its state.
 *
 * @details Runs the chain of the coupler, which writes the trigger, then the pairing.
 *   On a change of state:
 *   - COUPLED: the link is on, with a new startup grace;
 *   - PAIRING: only the coupling frames pass;
 *   - DETACHED, CONFLICT or FAILED: the link is off, nothing is sent or received, the
 *     contract is UNKNOWN and the received positions are back to their safe value.
 *
 * @param rt       Storage of the link; `rt.coupling.dev` is set.
 * @param linkIdx  Index of the link.
 * @param bus      ComBus of the node.
 */

void combus_coupling_follow( ComLinkRuntime& rt, uint8_t linkIdx, ComBus& bus );


/**
 * @brief Write the contributors of a coupled link, once per cycle.
 *
 * @details `fail`: true when hitched without a sound link (CONFLICT, FAILED, or COUPLED
 *   and lost).  It stays false while PAIRING.  `coupled`: true when COUPLED and, if the
 *   coupler has a detector, the detector settled.
 *
 * @param rt    Storage of the link; `rt.coupling.dev` is set.
 * @param bus   ComBus of the node.
 * @param lost  The link is lost, or never seen once the grace has elapsed.
 */

void combus_coupling_publish( ComLinkRuntime& rt, ComBus& bus, bool lost );



// =============================================================================
// 2. PASSIVE COUPLER
// =============================================================================

/**
 * @brief A passive coupler: a local instance, with no link and no ComDev.
 */

struct ComCouplingRuntime {
	const CouplingDev* dev     = nullptr;
	ComLinkPairing     pairing = {};
};


/**
 * @brief Put the passive couplers of the machine under watch.
 *
 * @param couplings  Static storage, owned by the caller.
 * @param count      Number of entries in @p couplings.
 */

void combus_couplings_watch( ComCouplingRuntime* couplings, uint8_t count );


/**
 * @brief Step the passive couplers, once per cycle.
 *
 * @details For each one: the chain of the coupler if any, then its trigger (the
 *   detector) with its debounce.  COUPLED once settled, with no pairing;
 *   CouplingDev::coupled is written.
 *
 * @param bus  ComBus of the node.
 */

void combus_couplings_update( ComBus& bus );


/**
 * @brief State of a passive coupler.
 *
 * @param idx  Index of the coupler.
 *
 * @return COUPLED or DETACHED; DETACHED for an index out of range.
 */

PairingState combus_couplings_state( uint8_t idx );


/**
 * @brief Identity of a passive coupler.
 *
 * @param idx  Index of the coupler.
 *
 * @return The identity read or by default at the coupling, cleared at the release; empty
 *   for an index out of range.
 */

CouplingId combus_couplings_id( uint8_t idx );



// =============================================================================
// 3. SNAPSHOTS FOR THE DASHBOARDS
// =============================================================================

/**
 * @brief Number of passive couplers under watch.
 *
 * @return The number of couplers; 0 if there is no watch.
 */

uint8_t combus_couplings_count( void );


/**
 * @brief Snapshot of a passive coupler.
 *
 * @param idx  Index of the coupler.
 * @param out  Copy of its state.
 *
 * @return false if @p idx is out of range.
 */

bool combus_couplings_view( uint8_t idx, ComCouplingRuntime& out );

// EOF combus_coupling_watch.h
