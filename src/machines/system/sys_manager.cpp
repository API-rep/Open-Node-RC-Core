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
#include <core/system/inputs/remote_link_fallback_chain.h>  // chantier 12.5: REMOTE_LINK_LOST → IDLE fallback


// =============================================================================
// 1. API IMPLEMENTATION
// =============================================================================

void sys_manager_update(ComBus& bus) {

      // --- 1. Input acquisition (writes to ComBus channels) ---
    input_refresh();     // core: physical acquisition
    input_update(bus);   // machine: mapping -> ComBus

      // --- 2. vbat update (senses + re-arms FAILSAFE_VBAT) ---
    vbat_update();       // vbat_sense_tick() then FAILSAFE_VBAT = vbat_is_low(0)

      // --- 3. Failsafe chain (publishes DigitalComBusID::FAILSAFE) ---
      // Runs after inputs (so sub-combus reads are fresh) and after vbat_update
      // (which has re-armed FAILSAFE_VBAT).  Consumers below may now read
      // comBus.digitalBus[DigitalComBusID::FAILSAFE] for the current cycle.
      //
      // Chantier 12.6: the historical open-drain pre-clear of `bus.isNotDrived`
      // (FS1) is REMOVED — link health is now monitored at transport level by
      // per-link contributors (PS4_DS4_BT_LINK_LOST, UART_LINK_LOST, ...) and
      // aggregated by the remote_link_fallback_chain into REMOTE_LINK_LOST.
    failsafe_update(bus);

      // --- 4. Chantier 12.5 — REMOTE_LINK_LOST → IDLE fallback chain ---
      // Runs AFTER the failsafe aggregator and BEFORE the FSM.  When the
      // project defines HAS_REMOTE_LINK_LOST_FALLBACK, this chain watches
      // REMOTE_LINK_LOST and forces RUNLEVEL = IDLE on the rising edge.
      // The call itself is gated so we don't pay the cost of the no-op
      // façade on autonomous machines (no FS contributors).  See
      // failsafe_module.md §12.5 (new design).
#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)
    remote_link_fallback_update(bus);
#endif
}


// Chantier 12.6: `sys_manager_reset(ComBus&)` REMOVED — its only job was
// to re-arm the open-drain `isNotDrived` flag, which no longer exists.
// Link-health reset is now handled per-link by the contributors (each
// backend re-arms its own *_LINK_LOST channel every cycle). See
// failsafe_module.md §12.5 (new design).

// Keep a placeholder for source-level compatibility with out-of-tree
// callers (sound node, etc.). The function is a no-op now.
void sys_manager_reset(ComBus& /*bus*/) {
    // Intentionally empty — see comment above.
}

// EOF sys_manager.cpp
