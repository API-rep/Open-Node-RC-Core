/*!****************************************************************************
 * @file  direct_drive_config.h
 * @brief Dumper truck — INPUT direct-drive chain: cfg, state, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbBtnCfg, CbBtnState, CbBtnTrigger, CbProc, cb_btn_toggle_fn,
 *     DigitalComBusID.
 *
 *   Chain function: DIRECT_DRIVE_BTN → [toggle] → DIRECT_DRIVE.
 *   In:  DIRECT_DRIVE_BTN (OPTIONS button).
 *   Out: DIRECT_DRIVE (flag — 0 = normal mode, 1 = direct-drive active).
 *
 *   1  toggle: OPTIONS button — enable/disable direct-drive mode.
 *      When active, bypass procs in SIM chains skip their ramp.
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  DIRECT_DRIVE toggle — enable/disable direct-drive mode (OPTIONS button).
  ///  bound=1 : when toggling "on", DIRECT_DRIVE is set to 1 (active).
static constexpr CbBtnCfg kDirectDriveToggleCfg {
    .bound   = 1u,                          ///< "On" value — direct-drive active.
    .holdMs  = 0u,                          ///< Immediate press.
    .trigger = CbBtnTrigger::ON_PRESS,
};


// =============================================================================
// 2. STATE
// =============================================================================

static CbBtnState gDirectDriveToggleState {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Direct-drive chain proc array.
 * @details Steps:
 *   1  toggle : OPTIONS button — 0 ↔ 1 (enable/disable direct-drive mode).
 */
static CbProc kDirectDriveProcs[] = {
      // 1. toggle — OPTIONS button: 0 ↔ 1
    { .name  = "direct_drive_toggle",
      .inCh  = DigitalComBusID::DIRECT_DRIVE_BTN,
      .fn    = cb_btn_toggle_fn,
      .cfg   = &kDirectDriveToggleCfg,
      .state = &gDirectDriveToggleState,
    },
};

// EOF direct_drive_config.h
