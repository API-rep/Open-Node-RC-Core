/*!****************************************************************************
 * @file  test_coupler_chain.h
 * @brief Chain of the TEST coupler, vehicle end (link rework, step 3.4e).
 *
 * @details Temporary form until the declared chains (.cbch, WIP "Simulation
 *   chain"): a C++ chain in the folder of the coupler, guarded by the flag of
 *   the coupler and the nature of the node.  The machine points its
 *   CouplingDev::chain to it.
 *******************************************************************************///
#pragma once

#if defined(COMBUS_COUPLER_TEST_COUPLER) && defined(IS_VEHICLE)

#include <core/system/combus/combus_defs.h>   // ComBus


/**
 * @brief Latch the coupling button into the coupling command (the trigger).
 *
 * @details TEST_COUPLER_BTN (input of the machine type, from the remote)
 *   toggles TEST_COUPLER_CMD on each press, at once (ON_PRESS, no hold).
 *   Never ON_RELEASE: at the loss of the remote, the button takes back its
 *   safe value (false), which would read as a release and uncouple.  With
 *   HAS_TEST_COUPLER_ENGAGED_SW, coupling (false to true) needs the engaged
 *   switch, settled for kTestCouplerEngagedSettleMs; uncoupling never does.
 */

void test_coupler_vehicle_chain( ComBus& bus );


/**
 * @brief Forget the state of the chain (button and guard history): tests, re-init.
 */

void test_coupler_vehicle_chain_reset( void );


/// The engaged switch stays on this long before a coupling is accepted (cb_guard_fn).
static constexpr uint16_t kTestCouplerEngagedSettleMs = 200u;

#endif

// EOF test_coupler_chain.h
