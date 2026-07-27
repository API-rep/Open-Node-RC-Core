/*!****************************************************************************
 * @file  steering_config.h
 * @brief Dumper truck — SIM steering chain: cfg, state, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbRampCfg, CbRampState, CbProc, cb_sym_ramp_fn, cb_bypass_fn,
 *     DigitalComBusID, CbusNeutral, pctToCbus.
 *
 *   Chain function: STEERING_BUS → [bypass?] → ramp → STEERING_RAMPED_BUS.
 *   In:  STEERING_BUS (raw stick), DIRECT_DRIVE.
 *   Out: STEERING_RAMPED_BUS (ramped steering for servo).
 *
 *   bypass: DIRECT_DRIVE HIGH → claim; raw steering skips ramp (direct servo control).
 *   ramp: progressive start (5 %/tick at 20 ms), instant stop (brakeSteps = CbusNeutral).
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  Steering — progressive start (5 %/tick @ 20 ms), instant stop.
static constexpr CbRampCfg kSteerAsymRamp {
    .rampTimeMs  = 20u,
    .accelSteps  = pctToCbus(5),
    .brakeSteps  = CbusNeutral,
    .neutralBand = 0u,
};


// =============================================================================
// 2. STATE
// =============================================================================

static CbRampState gSteerRampState {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Steering chain proc array.
 * @details Steps:
 *   1  bypass : DIRECT_DRIVE HIGH → claim; raw steering skips ramp.
 *   2  ramp   : progressive start (5 %/tick), instant stop (brakeSteps = CbusNeutral).
 */

static CbProc kSteeringProcs[] = {
      // 1. bypass — DIRECT_DRIVE HIGH → claim; raw steering skips ramp.
    { .name    = "bypass",
      .inCh    = DigitalComBusID::DIRECT_DRIVE,
      .fn      = cb_bypass_fn,
    },
      // 2. ramp — progressive start (5 %/tick), instant stop.
    { .name  = "ramp",
      .fn    = cb_sym_ramp_fn,
      .cfg   = &kSteerAsymRamp,
      .state = &gSteerRampState,
    },
};

// EOF steering_config.h
