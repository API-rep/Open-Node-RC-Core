/******************************************************************************
 * @file switch.h
 * @brief Polling-based switch debounce — board config driven.
 *
 * @details Manages a board-configured array of digital input switches with
 *   per-channel debounce.  Configuration is provided via SwitchPort (defined
 *   in machines_struct.h), the same ptr+count+state container pattern used by
 *   VBatSense, Board, and Machine.  No fixed channel limit — the count is
 *   driven entirely by the board config.
 *
 *   Registration : call switch_init() once with the board SwitchPort pointer.
 *   Runtime      : call switch_update() every loop iteration.
 *   Read         : switch_read(idx)  or  port->state[idx].confirmed.
 *****************************************************************************/

#pragma once

#include <struct/machines_struct.h>

// =============================================================================
// 1. API
// =============================================================================

/// Configure pin modes and seed initial states from the board config.
/// @param port  Board-defined SwitchPort container (must outlive all calls).
void switch_init(SwitchPort* port);

/// Poll all registered switches and commit debounced state changes.
/// Must be called once per loop iteration.
void switch_update();

/// Return the current debounced digitalRead() level for switch at @p idx.
bool switch_read(uint8_t idx);

/// Return the number of registered switches (0 before switch_init).
uint8_t switch_count();

// EOF switch.h
