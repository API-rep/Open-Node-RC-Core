/******************************************************************************
 * @file switch_dev.h
 * @brief Polling-based switch debounce — board config driven.
 *
 * @details Manages a board-configured array of digital input switches with
 *   per-channel debounce.  Configuration is provided via SwitchPort (defined
 *   in machines_struct.h), the same ptr+count+state container pattern used by
 *   VBatSense, Board, and EnvCfg.  No fixed channel limit — the count is
 *   driven entirely by the board config.
 *
 *   Registration : call switchDevInit() once with the board SwitchPort pointer.
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
void switchDevInit(SwitchPort* port);

/// Poll all registered switches and commit debounced state changes.
/// Must be called once per loop iteration.
void switch_update();

/// Return the current debounced digitalRead() level for switch at @p idx.
bool switch_read(uint8_t idx);

/// Return the number of registered switches (0 before switchDevInit).
uint8_t switch_count();

// EOF switch_dev.h
