/******************************************************************************
 * @file sys_manager.cpp
 * @brief System-level tick orchestrator — implementation.
 *****************************************************************************/

#include "sys_manager.h"

#include <core/system/input/input_manager.h>    // input_refresh() — core acquisition
#include <machines/system/input/input_update.h> // input_update(bus) — machine mapping
#include <core/system/vbat/vbat_sense.h>


// =============================================================================
// 1. API IMPLEMENTATION
// =============================================================================

SysResult sys_manager_update(ComBus& bus) {

      // --- 1. Open-drain pre-clear ---
    bus.isDrived = false;

      // --- 2. Input acquisition (re-asserts isDrived if source active) ---
    input_refresh();     // core: physical acquisition
    input_update(bus);   // machine: mapping -> ComBus

      // --- 3. Battery sensing tick ---
    bool vbatChanged = vbat_sense_tick();

      // --- 4. Failsafe evaluation ---
    return { !bus.isDrived, vbatChanged };
}


void sys_manager_reset(ComBus& bus) {
    bus.isDrived = false;
}

// EOF sys_manager.cpp