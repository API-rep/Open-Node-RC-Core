/******************************************************************************
 * @file dashboard_sig.h
 * @brief ANSI terminal dashboard — Layer 3 signal device module view.
 *
 * @details Shows live ComBus values per signal device (slot view) and
 *   the static device configuration snapshot captured at init (detail view).
 *   Compiled only when -D DEBUG_DASHBOARD is set.
 *****************************************************************************/
#pragma once

#ifdef DEBUG_DASHBOARD

#include <stdint.h>
#include <struct/combus_struct.h>
#include <struct/machines_struct.h>

/**
 * @brief Register the signal-device view slot with the core dashboard.
 *
 * @details Stores data pointers and calls dashboard_register_slot('5', ...).
 *   Must be called from dashboard_machine_setup() after dashboard_setup().
 *
 * @param bus   Pointer to the active ComBus instance.
 * @param mach  Pointer to the Machine config.
 */
void dashboard_sig_register(const ComBus* bus, const Machine* mach);

#else // !DEBUG_DASHBOARD
inline void dashboard_sig_register(const void*, const void*) {}
#endif // DEBUG_DASHBOARD

// EOF dashboard_sig.h
