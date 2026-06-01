/*!****************************************************************************
 * @file  traction_config.h
 * @brief Dumper truck — SIM traction chain: proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbProc, gear_subgear_cap_fn, gear_dir_fn, AnalogComBusID,
 *     kGearCfg (from gear_config.h — must be included before this file). *
 *   Chain function: ESC_RPM_BUS (engine_rpm, from GEAR chain) → subgear cap → direction → ESC_SPEED_BUS.
 *   In:  ESC_RPM_BUS (engine_rpm), SUBGEAR_BUS, DRIVE_STATE_BUS.
 *   Out: ESC_SPEED_BUS (bipolar wheel_speed for ESC).
 *
 *   subgear-cap: SUBGEAR active → cap magnitude to maxSpeedPct of gear-1 ratio; passthrough if 0.
 *   gear-dir: applies DRIVE_STATE_BUS sign → bipolar ESC_SPEED_BUS (FWD positive, REV negative). *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. PROC ARRAY
// =============================================================================

/**
 * @brief  Traction chain proc array.
 * @details Steps:
 *   1  subgear-cap : SUBGEAR active → cap magnitude to maxSpeedPct; passthrough if SUBGEAR_BUS = 0.
 *   2  gear-dir    : apply DRIVE_STATE_BUS sign → bipolar ESC_SPEED_BUS.
 */

static CbProc kTractionProcs[] = {
      // 1. subgear-cap — SUBGEAR active → cap magnitude to maxSpeedPct; passthrough if 0.
    { .name  = "subgear-cap",
      .inCh  = AnalogComBusID::SUBGEAR_BUS,
      .fn    = gear_subgear_cap_fn,
      .cfg   = &kGearCfg,
    },
      // 2. gear-dir — apply DRIVE_STATE_BUS sign → bipolar ESC_SPEED_BUS.
    { .name  = "gear-dir",
      .inCh  = AnalogComBusID::DRIVE_STATE_BUS,
      .fn    = gear_dir_fn,
      .cfg   = &kGearCfg,
    },
};

// EOF traction_config.h
