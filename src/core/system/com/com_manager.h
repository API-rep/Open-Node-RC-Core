/******************************************************************************
 * @file  com_manager.h
 * @brief Communication update dispatcher: reception and emission in the main loop.
 *
 * @details Two call points for the communication modules:
 *   - com_rx_update(): reception, BEFORE the application logic: the frames received
 *     are applied to the ComBus and the health of the links is published;
 *   - com_update(): emission, AFTER the application logic, called by output_update().
 *
 *   Each communication module is dispatched from here, conditioned on its build flag.
 *   Both are no-ops when no module is enabled.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>



// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Receive on every receiving link, and report the loss of each link.
 *
 * @details Non-blocking.  Reads the receiving links, which apply the frames they decode
 *   to @p bus, merges the both_or channels and logs the loss and the recovery of each
 *   link.  Then writes NODE_LINK_FAIL from the links to another node, and BOARD_LINK_FAIL
 *   from the links between boards; false when there is no such link.
 *
 *   Call once per main loop iteration, BEFORE the failsafe chains and the application
 *   logic.
 *
 * @param bus  ComBus of the node, written by the frames received.
 */

void com_rx_update(ComBus& bus);


/**
 * @brief Dispatch the emission of every active communication module.
 *
 * @details Non-blocking: each module manages its own timing.  Call once per main loop
 *   iteration, from output_update().
 *
 * @param bus  ComBus of the node, read only.
 */

void com_update(const ComBus& bus);

// EOF com_manager.h
