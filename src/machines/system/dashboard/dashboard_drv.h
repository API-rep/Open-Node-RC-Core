/******************************************************************************
 * @file dashboard_drv.h
 * @brief ANSI terminal dashboard — Layer 3 DC-driver module view.
 *
 * @details Shows live combus command values per DC device (top section) and
 *   the static device configuration snapshot captured at init (bottom section).
 *   Compiled only when -D DEBUG_DASHBOARD is set.
 *****************************************************************************/
#pragma once

#ifdef DEBUG_DASHBOARD

#include <stdint.h>
#include <struct/combus_struct.h>
#include <struct/machines_struct.h>

/**
 * @brief Register the DC-driver view slot with the core dashboard.
 *
 * @details Stores data pointers and calls dashboard_register_slot('2', ...).
 *   Must be called from dashboard_machine_setup() after dashboard_setup().
 *
 * @param bus   Pointer to the active ComBus instance.
 * @param mach  Pointer to the Machine config.
 */
void dashboard_drv_register(const ComBus* bus, const Machine* mach);

#else // !DEBUG_DASHBOARD
inline void dashboard_drv_register(const void*, const void*) {}
#endif // DEBUG_DASHBOARD

// EOF dashboard_drv.h
