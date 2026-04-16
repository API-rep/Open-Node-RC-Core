/******************************************************************************
 * @file combus_access.h
 * @brief Ownership-checked write accessors for ComBus channels.
 *
 * @details Each write function compares the caller's `ChanOwner` identity
 *   against the channel's declared `owner` before writing. If they do not
 *   match, the write is skipped and the function returns `false`.
 *   In DEBUG_COMBUS builds a warning is also printed.
 *
 *   This layer is optional — existing direct field writes remain valid during
 *   the migration. New code should use these accessors so ownership violations
 *   are visible at runtime.
 *
 *   Rules:
 *   - `ChanOwner::NONE`  — no module is mandated; write is denied for all callers.
 *   - `ChanOwner::ANY`   — all modules are mandated; any caller may write.
 *   - Any other value    — only the caller whose identity matches may write.
 *****************************************************************************/
#pragma once

#include <struct/combus_struct.h>
#include <core/config/machines/combus_ids.h>


// =============================================================================
// 1. NODE GROUP
// =============================================================================

/**
 * @brief Set the NodeGroup of this node — must be called once in combus_init().
 * @param group  One of ComBusOwner::GRP_MACHINE, ComBusOwner::GRP_SOUND, ComBusOwner::GRP_REMOTE, …
 */
void combus_set_node_group(uint8_t group);


// =============================================================================
// 2. CHANNEL WRITE ACCESSORS
// =============================================================================

/**
 * @brief Write a value to an analog ComBus channel.
 * @param bus    Target ComBus instance.
 * @param ch     Channel index (typed enum, machine-specific).
 * @param val    New value to write.
 * @param caller Identity of the calling module — checked against ch.owner.
 * @return true if the write was accepted, false if ownership mismatch.
 */
bool combus_set_analog(ComBus& bus, AnalogComBusID ch, uint16_t val, ChanOwner caller);



/**
 * @brief Write a value to a digital ComBus channel.
 * @param bus    Target ComBus instance.
 * @param ch     Channel index (typed enum, machine-specific).
 * @param val    New value to write.
 * @param caller Identity of the calling module — checked against ch.owner.
 * 
 * @return true if the write was accepted, false if ownership mismatch.
 */

bool combus_set_digital(ComBus& bus, DigitalComBusID ch, bool val, ChanOwner caller);



// =============================================================================
// 2. HEADER FIELD WRITE ACCESSORS
// =============================================================================

/**
 * @brief Write the ComBus run level.
 * 
 * @param bus    Target ComBus instance.
 * @param rl     New RunLevel value.
 * @param caller Identity of the calling module — checked against bus.runLevelOwner.
 * 
 * @return true if the write was accepted, false if ownership mismatch.
 */

bool combus_set_runlevel(ComBus& bus, RunLevel rl, ChanOwner caller);



/**
 * @brief Write the ComBus keyOn flag.
 * 
 * @param bus    Target ComBus instance.
 * @param val    New keyOn value.
 * @param caller Identity of the calling module — checked against bus.keyOnOwner.
 * 
 * @return true if the write was accepted, false if ownership mismatch.
 */

bool combus_set_keyon(ComBus& bus, bool val, ChanOwner caller);



/**
 * @brief Write the ComBus batteryIsLow flag.
 * 
 * @param bus    Target ComBus instance.
 * @param val    New batteryIsLow value.
 * @param caller Identity of the calling module — checked against bus.battLowOwner.
 * 
 * @return true if the write was accepted, false if ownership mismatch.
 */

bool combus_set_battlow(ComBus& bus, bool val, ChanOwner caller);

// EOF combus_access.h
