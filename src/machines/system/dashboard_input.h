/******************************************************************************
 * @file dashboard_input.h
 * @brief ANSI terminal dashboard — Layer 3 inputs/combus module view.
 *
 * @details Shows live analog and digital channel values (top section) and a
 *   combus runtime state summary (bottom section).
 *   Compiled only when -D DEBUG_DASHBOARD is set.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/combus_struct.h>

#ifdef DEBUG_DASHBOARD

/**
 * @brief Register the inputs/combus view slot with the core dashboard.
 *
 * @details Stores data pointers and calls dashboard_register_slot('1', ...).
 *   Must be called from dashboard_machine_setup() after dashboard_setup().
 *
 * @param bus        Pointer to the active ComBus instance.
 * @param analogCh   Total number of analog combus channels.
 * @param digitalCh  Total number of digital combus channels.
 */
void dashboard_input_register(const ComBus* bus, uint8_t analogCh, uint8_t digitalCh);

#else // !DEBUG_DASHBOARD
inline void dashboard_input_register(const ComBus*, uint8_t, uint8_t) {}
#endif // DEBUG_DASHBOARD

// EOF dashboard_input.h
