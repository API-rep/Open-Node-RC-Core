/******************************************************************************
 * @file output_manager.h
 * @brief Umbrella output update dispatcher.
 *
 * @details Single call point for all output-side updates in the main loop.
 * The communication modules are dispatched through com_update()
 * (core/system/com/com_manager.h); future output modules (local devices) are
 * added here, conditioned on their build flag.
 * Degrades gracefully to a no-op when no output module is enabled.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>


// =============================================================================
// 1. PUBLIC API
// =============================================================================

/**
 * @brief Dispatch all active output module updates.
 *
 * @details Non-blocking — each sub-module manages its own timing internally.
 * Call once per main loop iteration after the RunLevel state machine.
 *
 * @param bus           Current ComBus snapshot (read-only).
 */
void output_update(const ComBus& bus);

// EOF output_manager.h
