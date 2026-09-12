/******************************************************************************
 * @file sys_manager.cpp
 * @brief System-level tick orchestrator — implementation.
 *****************************************************************************/

#include "sys_manager.h"

#include <core/system/input/input_manager.h>    // input_refresh() — core acquisition
#include <machines/system/input/input_update.h> // input_update(bus) — machine mapping
#include <core/system/vbat/vbat.h>              // vbat_update() — orchestrates vbat_sense + FAILSAFE_VBAT
#include <core/system/failsafe/failsafe.h>      // failsafe_update() — publishes DigitalComBusID::FAILSAFE
#include <core/system/combus/combus_access.h>   // combus_set_digital() — FS1 re-arm FAILSAFE_COMBUS_LINK
#include <core/config/machines/combus_types.h>  // DigitalComBusID::FAILSAFE_COMBUS_LINK (FS1)


// =============================================================================
// 1. API IMPLEMENTATION
// =============================================================================

void sys_manager_update(ComBus& bus) {

      // --- 1. Open-drain pre-clear (FS1 — inverted semantics) ---
      // isNotDrived = true means "no source refreshed the bus this cycle" (fault).
      // Pre-set to true (failsafe-by-default), cleared by each active source.
    bus.isNotDrived = true;

      // --- 2. Input acquisition (clears isNotDrived if source active) ---
    input_refresh();     // core: physical acquisition
    input_update(bus);   // machine: mapping -> ComBus

      // --- 3. vbat update (senses + re-arms FAILSAFE_VBAT) ---
    vbat_update();       // vbat_sense_tick() then FAILSAFE_VBAT = vbat_is_low(0)

      // --- 4. FS1 — Re-arm FAILSAFE_COMBUS_LINK (proof-of-life for the Failsafe aggregator) ---
      // isNotDrived == true  => FAILSAFE_COMBUS_LINK = true  (fault)
      // isNotDrived == false => FAILSAFE_COMBUS_LINK = false (healthy)
      // The aggregator consumes this each cycle (see failsafe_chain.cpp).
    combus_set_digital(bus, DigitalComBusID::FAILSAFE_COMBUS_LINK, bus.isNotDrived, ChanLayer::LOCAL);

      // --- 5. Failsafe chain (publishes DigitalComBusID::FAILSAFE) ---
      // Runs after inputs (so sub-combus reads are fresh) and after vbat_update
      // (which has re-armed FAILSAFE_VBAT).  Consumers below may now read
      // comBus.digitalBus[DigitalComBusID::FAILSAFE] for the current cycle.
    failsafe_update(bus);
}


void sys_manager_reset(ComBus& bus) {
    bus.isNotDrived = true;
}

// EOF sys_manager.cpp
