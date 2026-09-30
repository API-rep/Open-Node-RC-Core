/*!****************************************************************************
 * @file  runlevel_procs.h
 * @brief Dumper truck — RUNLEVEL proc array: cfg, state, proc array.
 *
 * @details Private include — included only from runlevel_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbRunlevelCfg, CbProc, cb_runlevel_fn,
 *     DigitalComBusID, RunLevel, comBus.
 *
 *   Chain function: FAILSAFE + KEY_ACTIVE → RUNLEVEL.
 *
 *   1  failsafe : FAILSAFE=true → RUNLEVEL=RunLevel::FAILSAFE (= IDLE) + claim.
 *                 Blocks the keyboard runlevel proc when failsafe is active.
 *   2  runlevel : KEY_ACTIVE=true  → RUNLEVEL=STARTING
 *                 KEY_ACTIVE=false → RUNLEVEL=TURNING_OFF
 *                 (writes every cycle while lever is held — no edge detection).
 *
 *   In:  none (each proc reads its own inCh).
 *   Out: none (side-effect: writes RUNLEVEL analog channel).
 *******************************************************************************
 */
#pragma once

#include <machines/config/machines/volvo_A60H_bruder/combus/combus.h>   // comBus
#include <core/system/combus/processors/base/cb_runlevel.h>             // CbRunlevelCfg, cb_runlevel_fn


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  failsafe — FAILSAFE=true → force IDLE + claim (blocks keyboard runlevel).
static constexpr CbRunlevelCfg kFailsafeRunlevelCfg {
    .bus   = &comBus,
    .high  = RunLevel::FAILSAFE,   // = IDLE (alias)
    .low   = std::nullopt,         // do not write when FAILSAFE=false
    .claim = true,                 // block downstream procs
};

  ///  runlevel — KEY_ACTIVE → STARTING / TURNING_OFF.
static constexpr CbRunlevelCfg kKeyRunlevelCfg {
    .bus   = &comBus,
    .high  = RunLevel::STARTING,
    .low   = RunLevel::TURNING_OFF,
    .claim = false,
};


// =============================================================================
// 2. PROC ARRAY
// =============================================================================

/**
 * @brief  RunLevel chain proc array.
 * @details Steps:
 *   1  failsafe : FAILSAFE=true → RUNLEVEL=IDLE + claim (blocks step 2).
 *   2  runlevel : KEY_ACTIVE → RUNLEVEL (STARTING / TURNING_OFF).
 */
static CbProc kRunlevelProcs[] = {
      // 1. failsafe — FAILSAFE=true → force IDLE + claim.
    { .name  = "failsafe",
      .inCh  = DigitalComBusID::FAILSAFE,
      .fn    = cb_runlevel_fn,
      .cfg   = &kFailsafeRunlevelCfg,
      .state = nullptr,
    },
      // 2. runlevel — KEY_ACTIVE → RUNLEVEL (skipped when failsafe claims).
    { .name  = "runlevel",
      .inCh  = DigitalComBusID::KEY_ACTIVE,
      .fn    = cb_runlevel_fn,
      .cfg   = &kKeyRunlevelCfg,
      .state = nullptr,
    },
};

// EOF runlevel_procs.h