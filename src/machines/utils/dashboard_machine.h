/******************************************************************************
 * @file dashboard_machine.h
 * @brief ANSI terminal dashboard — Layer 2 machine environment entry point.
 *
 * @details Owns the overview view, auto-registers module slots, and stores
 *   machine-level data pointers.  This is the single setup call for the full
 *   dashboard stack from machine application code.
 *
 *   Call hierarchy triggered by dashboard_machine_setup():
 *     dashboard_setup()              — Layer 1 core reset
 *     dashboard_register_slot('0')   — overview (this file)
 *     dashboard_input_register()     — Layer 3 inputs view
 *     dashboard_drv_register()       — Layer 3 drivers view
 *     dashboard_vbat_register()      — Layer 3 battery view
 *
 *   Compiled only when -D DEBUG_DASHBOARD is set.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/combus_struct.h>
#include <struct/machines_struct.h>

#ifdef DEBUG_DASHBOARD

/**
 * @brief Full dashboard stack setup — the only dashboard call from init.cpp.
 *
 * @details Resets core state, registers all module slots, and pushes an
 *   initial event.  Must be called once after all other init sequences
 *   complete, before the first dashboard_update() in loop().
 *
 * @param bus        Pointer to the active ComBus instance.
 * @param mach       Pointer to the active Machine config.
 * @param analogCh   Total number of analog combus channels.
 * @param digitalCh  Total number of digital combus channels.
 */
void dashboard_machine_setup(const ComBus* bus, const Machine* mach,
                              uint8_t analogCh, uint8_t digitalCh);

#else // !DEBUG_DASHBOARD
inline void dashboard_machine_setup(const ComBus*, const Machine*, uint8_t, uint8_t) {}
#endif // DEBUG_DASHBOARD

// EOF dashboard_machine.h
