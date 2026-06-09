/*!****************************************************************************
 * @file  traction_config.h
 * @brief Dumper truck — SIM traction chain: proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbProc, cb_bypass_fn, gear_subgear_cap_fn, gear_dir_fn, AnalogComBusID, DigitalComBusID,
 *     kGearCfg (from gear_config.h — must be included before this file). *
 *   Chain function: ESC_RPM_BUS (engine_rpm, from GEAR chain) → bypass → subgear cap → direction → ESC_SPEED_BUS.
 *   In direct mode bypass claims at pos 1; ESC_RPM_BUS = raw THROTTLE_BUS (bipolar) → ESC_SPEED_BUS direct.
 *   In:  ESC_RPM_BUS (engine_rpm), SUBGEAR_BUS, DRIVE_STATE_BUS.
 *   Out: ESC_SPEED_BUS (bipolar wheel_speed for ESC).
 *
 *   bypass: DIRECT_DRIVE HIGH → claim; ESC_RPM_BUS (= raw THROTTLE_BUS in direct mode) written to ESC_SPEED_BUS.
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
 *   1  bypass     : DIRECT_DRIVE HIGH → claim; ESC_RPM_BUS written to ESC_SPEED_BUS direct.
 *   2  subgear-cap : SUBGEAR active → cap magnitude to maxSpeedPct; passthrough if SUBGEAR_BUS = 0.
 *   3  gear-dir    : apply DRIVE_STATE_BUS sign → bipolar ESC_SPEED_BUS.
 */

static CbProc kTractionProcs[] = {
      // 1. bypass — DIRECT_DRIVE HIGH → claim; ESC_RPM_BUS (= raw THROTTLE_BUS) written to ESC_SPEED_BUS.
    { .name  = "bypass",
      .inCh  = DigitalComBusID::DIRECT_DRIVE,
      .fn    = cb_bypass_fn,
    },
      // 2. subgear-cap — SUBGEAR active → cap magnitude to maxSpeedPct; passthrough if 0.
    { .name  = "subgear-cap",
      .inCh  = AnalogComBusID::SUBGEAR_BUS,
      .fn    = gear_subgear_cap_fn,
      .cfg   = &kGearCfg,
    },
      // 3. gear-dir — apply DRIVE_STATE_BUS sign → bipolar ESC_SPEED_BUS.
    { .name  = "gear-dir",
      .inCh  = AnalogComBusID::DRIVE_STATE_BUS,
      .fn    = gear_dir_fn,
      .cfg   = &kGearCfg,
    },
};

// EOF traction_config.h
