/******************************************************************************
 * @file  sys_manager.cpp
 * @brief System step of a machine: every recurring system service, once per loop.
 *****************************************************************************/

#include "sys_manager.h"

#include <core/system/com/com_manager.h>                // com_rx_update()
#include <core/system/input/input_manager.h>            // input_refresh()
#include <machines/system/input/input_update.h>         // input_update()
#include <core/system/vbat/vbat.h>                      // vbat_update()
#include <core/system/failsafe/failsafe.h>              // failsafe_update()
#include <core/system/failsafe/failsafe_soft_chain.h>   // failsafe_soft_update()



// =============================================================================
// 1. API
// =============================================================================

/**
 * @brief System step. Each step reads what the previous ones wrote in this cycle.
 *
 * @details In order:
 *   0. Reception first: the link contributors must be fresh when the chains read them.
 *   1. Inputs: the physical acquisition, then the mapping to the ComBus.
 *   2. Battery: VBAT_FAIL.
 *   3. Hard chain, after every source of its contributors.
 *   4. Soft chain, after the hard one and before the state machine of main.cpp.
 */

void sys_manager_update(ComBus& bus) {

		// 0. Reception
	com_rx_update(bus);

		// 1. Inputs
	input_refresh();
	input_update(bus);

		// 2. Battery
	vbat_update(bus);

		// 3. Hard chain
	failsafe_update(bus);

		// 4. Soft chain
	failsafe_soft_update(bus);
}

// EOF sys_manager.cpp
