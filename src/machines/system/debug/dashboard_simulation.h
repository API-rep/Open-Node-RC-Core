/******************************************************************************
 * @file dashboard_simulation.h
 * @brief ANSI terminal dashboard — Layer 3 simulation module view.
 *
 * @details Main view: live table of all SimChannels — name, inCh→outCh
 *   mapping, live I/O values, bypass state, and active proc list.
 *   Detail sub-view: per-channel drill-down with live I/O and per-proc
 *   config snapshot (bypass condCh, ramp parameters, etc.).
 *   Compiled only when -D DEBUG_DASHBOARD is set.
 *****************************************************************************/
#pragma once

#ifdef DEBUG_DASHBOARD

#include <stdint.h>
#include <struct/combus_struct.h>
#include <struct/machines_struct.h>

/**
 * @brief Register the simulation view slot with the core dashboard.
 *
 * @details Stores data pointers and calls dashboard_register_slot('6', ...).
 *   Must be called from dashboard_machine_setup() after dashboard_setup().
 *
 * @param bus   Pointer to the active ComBus instance.
 * @param mach  Pointer to the EnvCfg config.
 */
void dashboard_simulation_register(const ComBus* bus, const EnvCfg* mach);

#else // !DEBUG_DASHBOARD
inline void dashboard_simulation_register(const void*, const void*) {}
#endif // DEBUG_DASHBOARD

// EOF dashboard_simulation.h
