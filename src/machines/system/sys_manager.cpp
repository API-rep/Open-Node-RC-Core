/******************************************************************************
 * @file sys_manager.cpp
 * @brief System-level tick orchestrator — implementation.
 *****************************************************************************/

#include "sys_manager.h"

#include <core/system/input/input_manager.h>    // input_refresh() — core acquisition
#include <machines/system/input/input_update.h> // input_update(bus) — machine mapping
#include <core/system/vbat/vbat.h>              // vbat_update() — orchestrates vbat_sense + FAILSAFE_VBAT
#include <core/system/failsafe/failsafe.h>      // failsafe_update() — publishes DigitalComBusID::FAILSAFE


// =============================================================================
// 1. API IMPLEMENTATION
// =============================================================================

SysResult sys_manager_update(ComBus& bus) {

      // --- 1. Open-drain pre-clear ---
    bus.isDrived = false;

      // --- 2. Input acquisition (re-asserts isDrived if source active) ---
    input_refresh();     // core: physical acquisition
    input_update(bus);   // machine: mapping -> ComBus

      // --- 3. vbat update (senses + re-arms FAILSAFE_VBAT) ---
    vbat_update();       // vbat_sense_tick() then FAILSAFE_VBAT = vbat_is_low(0)

      // --- 4. Failsafe chain (publishes DigitalComBusID::FAILSAFE) ---
      // Runs after inputs (so sub-combus reads are fresh) and after vbat_update
      // (which has re-armed FAILSAFE_VBAT).  Consumers below may now read
      // comBus.digitalBus[DigitalComBusID::FAILSAFE] for the current cycle.
    failsafe_update(bus);

      // --- 5. Legacy failsafe flag (kept until WIP §12.4/12.9/12.12 are done) ---
      // `failsafeActive` is the open-drain flag (no input source) — distinct from
      // the new aggregated `DigitalComBusID::FAILSAFE` (Failsafe chain output).
      // Both will coexist until the reaction chain migration is complete.
      //
      // `vbatChanged` is no longer used by main.cpp (the chain re-evaluates every
      // cycle), so it is hard-wired to false here. The legacy field stays in
      // SysResult for API compatibility until the migration is complete.
    return { !bus.isDrived, false };
}


void sys_manager_reset(ComBus& bus) {
    bus.isDrived = false;
}

// EOF sys_manager.cpp