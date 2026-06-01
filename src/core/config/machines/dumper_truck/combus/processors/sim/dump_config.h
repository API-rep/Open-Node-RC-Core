/*!****************************************************************************
 * @file  dump_config.h
 * @brief Dumper truck — SIM dump chain: cfg, state, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbRampCfg, CbRampState, CbProc, cb_sym_ramp_fn, cb_bypass_fn,
 *     DigitalComBusID, CbusNeutral, pctToCbus.
 *
 *   Chain function: DUMP_BUS → [bypass?] → asymmetric ramp → DUMP_RAMPED_BUS.
 *   In:  DUMP_BUS (raw actuator command), DIRECT_DRIVE.
 *   Out: DUMP_RAMPED_BUS (ramped dump command for hydraulic actuator).
 *
 *   bypass: DIRECT_DRIVE HIGH → claim; raw dump command skips ramp.
 *   ramp: slow raise (accelSteps = 2 %/tick), fast lower (accelDownSteps = 4 %/tick), instant stop.
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  Dump body — slow raise, fast lower, instant stop.
static constexpr CbRampCfg kDumpAsymRamp {
    .rampTimeMs     = 20u,
    .accelSteps     = pctToCbus(2),
    .accelDownSteps = pctToCbus(4),
    .brakeSteps     = CbusNeutral,
    .neutralBand    = 0u,
};


// =============================================================================
// 2. STATE
// =============================================================================

static CbRampState gDumpRampState {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Dump chain proc array.
 * @details Steps:
 *   1  bypass : DIRECT_DRIVE HIGH → claim; raw dump command skips ramp.
 *   2  ramp   : asymmetric — raise slow (2 %/tick), lower fast (4 %/tick), instant stop.
 */

static CbProc kDumpProcs[] = {
      // 1. bypass — DIRECT_DRIVE HIGH → claim; raw dump command skips ramp.
    { .name    = "bypass",
      .inCh    = DigitalComBusID::DIRECT_DRIVE,
      .fn      = cb_bypass_fn,
    },
      // 2. ramp — asymmetric: raise slow (2 %/tick), lower fast (4 %/tick).
    { .name  = "ramp",
      .fn    = cb_sym_ramp_fn,
      .cfg   = &kDumpAsymRamp,
      .state = &gDumpRampState,
    },
};

// EOF dump_config.h
