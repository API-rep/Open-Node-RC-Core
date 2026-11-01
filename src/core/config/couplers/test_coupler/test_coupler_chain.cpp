/******************************************************************************
 * @file  test_coupler_chain.cpp
 * @brief Chain of the TEST coupler, vehicle end (link rework, step 3.4e).
 *
 * @details See test_coupler_chain.h.  The pipeline is seeded with
 *   TEST_COUPLER_CMD and committed to it:
 *     0. engaged_delay (guard only) — cb_delay_fn: while uncoupled, the
 *                                     chain stops until the engaged switch
 *                                     has stayed on for its delay;
 *     1. toggle                     — TEST_COUPLER_BTN, ON_PRESS, no hold.
 *   The guard runs first because the toggle claims the chain when it fires.
 *   A button held while approaching couples once the switch has been on for the delay (the
 *   toggle then sees a new press); a short press without the switch does
 *   nothing; a switch that bounces never couples.  Uncoupling never needs the
 *   switch.
 *****************************************************************************/

#include "test_coupler_chain.h"

#if defined(HAS_COUPLER_SOCKET_TEST_COUPLER)

#include <core/system/combus/processors/proc_chain.h>        // proc_chain_step
#include <core/system/combus/processors/input/cb_btn.h>      // cb_btn_toggle_fn
#include <core/system/combus/processors/logic/cb_delay.h>    // cb_delay_fn
#include <core/config/machines/combus_types.h>               // DigitalComBusID


namespace {

const CbBtnCfg kToggleCfg = { .bound = 1u, .holdMs = 0u, .trigger = CbBtnTrigger::ON_PRESS };
CbBtnState     s_toggle   = {};

#if defined(HAS_TEST_COUPLER_ENGAGED_SW)
const CbDelayCfg kEngagedDelayCfg = { .delayMs = kTestCouplerEngagedDelayMs, .whileZeroOnly = true };
CbDelayState     s_engagedDelay   = {};
#endif

CbProc s_procs[] = {
#if defined(HAS_TEST_COUPLER_ENGAGED_SW)
    { .name  = "engaged_delay",
      .inCh  = DigitalComBusID::TEST_COUPLER_ENGAGED_SW,
      .fn    = cb_delay_fn,
      .cfg   = &kEngagedDelayCfg,
      .state = &s_engagedDelay },
#endif
    { .name  = "toggle",
      .inCh  = DigitalComBusID::TEST_COUPLER_BTN,
      .fn    = cb_btn_toggle_fn,
      .cfg   = &kToggleCfg,
      .state = &s_toggle },
};

CbChain s_chain = {
    "test_coupler_vehicle",
    DigitalComBusID::TEST_COUPLER_CMD,
    DigitalComBusID::TEST_COUPLER_CMD,
    s_procs,
    static_cast<uint8_t>(sizeof(s_procs) / sizeof(s_procs[0]))
};

} // namespace


void test_coupler_vehicle_chain( ComBus& bus )
{
    proc_chain_step(s_chain, bus);
}


void test_coupler_vehicle_chain_reset( void )
{
    s_toggle = CbBtnState{};
#if defined(HAS_TEST_COUPLER_ENGAGED_SW)
    s_engagedDelay = CbDelayState{};
#endif
}

#endif

// EOF test_coupler_chain.cpp
