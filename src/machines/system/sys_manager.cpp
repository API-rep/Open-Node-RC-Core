/******************************************************************************
 * @file sys_manager.cpp
 * @brief System-level tick orchestrator — implementation.
 *****************************************************************************/

#include "sys_manager.h"

#include <core/system/com/com_manager.h>        // com_rx_update() — reception and link health
#include <core/system/input/input_manager.h>    // input_refresh() — core acquisition
#include <machines/system/input/input_update.h> // input_update(bus) — machine mapping
#include <core/system/vbat/vbat.h>              // vbat_update() — orchestrates vbat_sense + VBAT_FAIL
#include <core/system/failsafe/failsafe.h>      // failsafe_update() — publishes DigitalComBusID::FAILSAFE_HARD
#include <core/system/failsafe/failsafe_soft_chain.h>  // chantier 12.5: FAILSAFE_SOFT → IDLE fallback


// =============================================================================
// 1. API IMPLEMENTATION
// =============================================================================

void sys_manager_update(ComBus& bus) {

	  // --- 0. Communication reception (applies received frames, publishes NODE_LINK_FAIL and BOARD_LINK_FAIL) ---
	  // First: the link health must be fresh when the FAILSAFE_SOFT chain (step 4) reads it.
	com_rx_update(bus);

	  // --- 1. Input acquisition (writes to ComBus channels) ---
	input_refresh();     // core: physical acquisition
	input_update(bus);   // machine: mapping -> ComBus

	  // --- 2. vbat update (senses + re-arms VBAT_FAIL) ---
	vbat_update(bus);    // vbat_sense_tick() then VBAT_FAIL = vbat_is_low(0)

	  // --- 3. Failsafe chain (publishes DigitalComBusID::FAILSAFE_HARD) ---
	  // Runs after inputs (so sub-combus reads are fresh) and after vbat_update
	  // (which has re-armed VBAT_FAIL).  Consumers below may now read
	  // comBus.digitalBus[DigitalComBusID::FAILSAFE_HARD] for the current cycle.
	  //
	  // Chantier 12.6: the historical open-drain pre-clear of `bus.isNotDrived`
	  // (FS1) is REMOVED — link health is now monitored at transport level by
	  // per-link contributors (LOCAL_REMOTE_FAIL, NODE_LINK_FAIL, ...) and
	  // aggregated by the failsafe_soft_chain into FAILSAFE_SOFT.
	failsafe_update(bus);

	  // --- 4. Chantier 12.5 — FAILSAFE_SOFT → IDLE fallback chain ---
	  // Runs AFTER the failsafe aggregator and BEFORE the FSM: the soft chain
	  // aggregates its contributors into FAILSAFE_SOFT, which the runlevel
	  // chain turns into RUNLEVEL = IDLE on the rising edge.  Part of the
	  // failsafe module (link rework S1b: HAS_FAILSAFE).  See
	  // failsafe_module.md §12.5 (new design).
	failsafe_soft_update(bus);
}

// EOF sys_manager.cpp
