/******************************************************************************
 * @file output_manager.h
 * @brief Umbrella output update dispatcher.
 *
 * @details Single call point for all output-side transport updates in the
 * main loop. Each output module (sound UART, future RF TX, etc.) is
 * dispatched from here, conditioned on its build flag.
 * Degrades gracefully to a no-op when no output module is enabled.
 *****************************************************************************/
#pragma once

#include <struct/combus_struct.h>


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
 * @param failsafeActive True when the input watchdog has triggered failsafe.
 */
void output_update(const ComBus& bus, bool failsafeActive);

// EOF output_manager.h
