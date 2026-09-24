/*!****************************************************************************
 * @file  runlevel_config.h
 * @brief Dumper truck — INPUT runlevel chain: cfg, state, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbRunlevelCfg, CbRunlevelOnceState, CbProc, cb_runlevel_fn,
 *     cb_runlevel_once_fn, AnalogComBusID, DigitalComBusID, RunLevel, comBus.
 *
 *   Chain function: RUNLEVEL → [failsafe, runlevel] → RUNLEVEL.
 *   In:  RUNLEVEL (current RunLevel value, seeded into pipeline value).
 *   Out: RUNLEVEL (committed after pipeline).
 *
 *   1  failsafe:  inCh = FAILSAFE.  CONTINUOUS (cb_runlevel_fn).
 *                 When FAILSAFE=true → write RUNLEVEL=FAILSAFE (dedicated value, see machines_defs.h)
 *                 and claim the chain (blocks runlevel proc).
 *                 When FAILSAFE=false → no write, no claim.
 *                 The dedicated RunLevel::FAILSAFE value triggers the
 *                 hardware safety reaction (stop/sleep/disable drivers)
 *                 in main.cpp's switch(curRunLevel).
 *
 *   2  runlevel:  inCh = KEY_ACTIVE.  EDGE-TRIGGERED (cb_runlevel_once_fn).
 *                 On KEY_ACTIVE 0→1 → write RUNLEVEL=STARTING (once).
 *                 On KEY_ACTIVE 1→0 → write RUNLEVEL=TURNING_OFF (once).
 *                 No claim (last proc in chain).
 *                 The edge-triggered variant is required so the FSM in
 *                 main.cpp can progress STARTING→RUNNING without being
 *                 overwritten back to STARTING on every cycle.
 *
 *   The chain uses read-modify-write: the runner seeds `value` from RUNLEVEL,
 *   each proc may overwrite it, and the final value is committed back to RUNLEVEL.
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  failsafe — FAILSAFE=true → force RUNLEVEL=FAILSAFE (dedicated value, see machines_defs.h), claim chain.
static constexpr CbRunlevelCfg kFailsafeRunlevelCfg {
    .high  = RunLevel::FAILSAFE,    ///< FAILSAFE=true → write FAILSAFE (dedicated value — triggers hardware safety in main.cpp).
    .low   = std::nullopt,          ///< FAILSAFE=false → no write.
    .claim = true,                  ///< Block runlevel proc when failsafe is active.
};

  ///  runlevel — KEY_ACTIVE drives STARTING/TURNING_OFF on edge transitions.
static constexpr CbRunlevelCfg kRunlevelCfg {
    .high  = RunLevel::STARTING,    ///< KEY_ACTIVE 0→1 → write STARTING.
    .low   = RunLevel::TURNING_OFF, ///< KEY_ACTIVE 1→0 → write TURNING_OFF.
    .claim = false,                 ///< Last proc — nothing to protect behind.
};


// =============================================================================
// 2. STATE
// =============================================================================

  //  runlevel edge-triggered state — prevValue initialised to 0 (no prior lever state).
static CbRunlevelOnceState gRunlevelOnceState {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Runlevel chain proc array.
 * @details Steps:
 *   1  failsafe : CONTINUOUS — FAILSAFE=true → RUNLEVEL=FAILSAFE + claim (blocks runlevel).
 *   2  runlevel : EDGE-TRIGGERED — KEY_ACTIVE transition → RUNLEVEL=STARTING or TURNING_OFF.
 */
static CbProc kRunlevelProcs[] = {
      // 1. failsafe — CONTINUOUS: FAILSAFE=true → force RunLevel::FAILSAFE (dedicated) + claim chain.
    { .name  = "failsafe",
      .inCh  = DigitalComBusID::FAILSAFE,
      .fn    = cb_runlevel_fn,
      .cfg   = &kFailsafeRunlevelCfg,
      .state = nullptr,
    },
      // 2. runlevel — EDGE-TRIGGERED: KEY_ACTIVE transition → STARTING/TURNING_OFF.
    { .name  = "runlevel",
      .inCh  = DigitalComBusID::KEY_ACTIVE,
      .fn    = cb_runlevel_once_fn,
      .cfg   = &kRunlevelCfg,
      .state = &gRunlevelOnceState,
    },
};

// EOF runlevel_config.h