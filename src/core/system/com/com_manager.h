/******************************************************************************
 * @file com_manager.h
 * @brief Communication update dispatcher.
 *
 * @details Single call point for all communication updates of the main loop.
 * Each communication module (ComBus UART TX, future transports) is dispatched
 * from here, conditioned on its build flag. Called by output_update().
 * Degrades gracefully to a no-op when no communication module is enabled.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Dispatch all active communication module updates.
 *
 * @details Non-blocking — each sub-module manages its own timing internally.
 * Call once per main loop iteration, from output_update().
 *
 * @param bus           Current ComBus snapshot (read-only).
 */

void com_update(const ComBus& bus);

// EOF com_manager.h
