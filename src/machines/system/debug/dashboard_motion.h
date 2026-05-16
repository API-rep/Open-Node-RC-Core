/******************************************************************************
 * @file dashboard_motion.h
 * @brief ANSI terminal dashboard — Layer 3 motion/inertia module view.
 *
 * @details Shows live inertia state per traction device (main view) and
 *   per-device config presets in the detail sub-view.  Only devices with a
 *   non-null MotionConfig pointer are displayed.
 *   Compiled only when -D DEBUG_DASHBOARD is set.
 *****************************************************************************/
#pragma once

#ifdef DEBUG_DASHBOARD

#include <stdint.h>
#include <struct/combus_struct.h>
#include <struct/machines_struct.h>

/**
 * @brief Register the motion/inertia view slot with the core dashboard.
 *
 * @details Stores data pointers and calls dashboard_register_slot('6', ...).
 *   Must be called from dashboard_machine_setup() after dashboard_setup().
 *
 * @param bus   Pointer to the active ComBus instance.
 * @param mach  Pointer to the EnvCfg config.
 */
void dashboard_motion_register(const ComBus* bus, const EnvCfg* mach);

#else // !DEBUG_DASHBOARD
inline void dashboard_motion_register(const void*, const void*) {}
#endif // DEBUG_DASHBOARD

// EOF dashboard_motion.h
