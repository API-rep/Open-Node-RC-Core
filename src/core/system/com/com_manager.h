/******************************************************************************
 * @file com_manager.h
 * @brief Communication update dispatcher.
 *
 * @details Two call points for the communication modules in the main loop:
 *   - com_rx_update(): reception, BEFORE the application logic — received
 *     frames are applied to the ComBus and the link health is published;
 *   - com_update(): emission, AFTER the application logic — called by
 *     output_update().
 *   Each communication module (ComBus UART, future transports) is dispatched
 *   from here, conditioned on its build flag. Both degrade gracefully to a
 *   no-op when no communication module is enabled.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Receive on every receiving link and publish the link health.
 *
 * @details Non-blocking. Drains the receiving links, which apply the frames
 * they decode to @p bus, then writes `UART_LINK_LOST` (BOARD scope): true while
 * a receiving link is silent or has not received anything yet, false otherwise
 * (and false when no link receives). Call once per main loop iteration, BEFORE
 * the REMOTE_LINK_LOST chain and the application logic.
 *
 * @param bus           Node ComBus, written by the received frames.
 */

void com_rx_update(ComBus& bus);



/**
 * @brief Dispatch all active communication module updates (emission).
 *
 * @details Non-blocking — each sub-module manages its own timing internally.
 * Call once per main loop iteration, from output_update().
 *
 * @param bus           Current ComBus snapshot (read-only).
 */

void com_update(const ComBus& bus);

// EOF com_manager.h
