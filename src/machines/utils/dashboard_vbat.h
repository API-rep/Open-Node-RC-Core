/******************************************************************************
 * @file dashboard_vbat.h
 * @brief ANSI terminal dashboard — Layer 3 battery sensing module view.
 *
 * @details Shows live voltage readings for all configured vbat channels.
 *   Renders a "not active" notice when VBAT_SENSING is not configured.
 *   Compiled only when -D DEBUG_DASHBOARD is set.
 *****************************************************************************/
#pragma once

#ifdef DEBUG_DASHBOARD

/**
 * @brief Register the battery view slot with the core dashboard.
 *
 * @details Calls dashboard_register_slot('3', "battery", ...).
 *   Must be called from dashboard_machine_setup() after dashboard_setup().
 */
void dashboard_vbat_register();

#else // !DEBUG_DASHBOARD
inline void dashboard_vbat_register() {}
#endif // DEBUG_DASHBOARD

// EOF dashboard_vbat.h
