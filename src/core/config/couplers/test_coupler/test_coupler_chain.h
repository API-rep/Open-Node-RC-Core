/*!****************************************************************************
 * @file  test_coupler_chain.h
 * @brief Chain of the TEST coupler, socket end.
 *
 * @details A C++ chain in the folder of the coupler, guarded by the flag of its side
 *   (the socket).  The machine points CouplingDev::chain to it; the watch of the
 *   couplings runs it before reading the trigger.
 *
 *   TODO(Simulation chain): temporary form, until the chains can be declared (.cbch).
 *******************************************************************************///
#pragma once

#if defined(HAS_COUPLER_SOCKET_TEST_COUPLER)

#include <core/system/combus/combus_defs.h>   // ComBus



/**
 * @brief Latch the coupling button into the coupling command, which is the trigger.
 *
 * @details TEST_COUPLER_BTN, an input of the machine type sent by the remote, toggles
 *   TEST_COUPLER_CMD on each press, at once (ON_PRESS, no hold).
 *
 *   Never ON_RELEASE: at the loss of the remote, the button takes back its safe value
 *   (false), which would read as a release and uncouple.
 *
 *   With HAS_TEST_COUPLER_ENGAGED_SW, coupling (false to true) needs the engaged
 *   switch, on for kTestCouplerEngagedDelayMs; uncoupling never does.
 *
 * @param bus  ComBus of the node.
 */

void test_coupler_socket_chain( ComBus& bus );


	/// Forget the state of the chain, button and delay history: tests, re-init.
void test_coupler_socket_chain_reset( void );


	/// The engaged switch stays on this long before a coupling is accepted (cb_delay_fn).
static constexpr uint16_t kTestCouplerEngagedDelayMs = 200u;

#endif

// EOF test_coupler_chain.h
