/*!****************************************************************************
 * @file  cruise_input_config.h
 * @brief Dumper truck — INPUT normal-cruise chain: cfg, state, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbBtnCfg, CbBtnState, CbBtnTrigger, CbProc, cb_btn_toggle_fn,
 *     cb_bypass_fn, CbBypassCfg, AnalogComBusID, DigitalComBusID.
 *
 *   Chain function: CRUISE_ACTIVE → [gate, toggle] → CRUISE_ACTIVE.
 *   In:  CRUISE_ACTIVE (current toggle state), SUBGEAR_BUS (gate signal),
 *        CRUISE_TOGGLE_BTN (□ button raw input).
 *   Out: CRUISE_ACTIVE (updated toggle state — read by SIM throttle sync proc).
 *
 *   1  gate   : SUBGEAR_BUS != 0 → claim with forceValue=0 (deactivate cruise
 *               while in subgear / crawler mode).
 *   2  toggle : □ button — 0 ↔ 1 (enable/disable normal cruise mode).
 *               Only runs when gate does not claim (i.e. SUBGEAR inactive).
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  CRUISE gate — force CRUISE_ACTIVE=0 when subgear is active.
  ///  forceValue=0 : output is set to 0 (inactive) when subgear is on.
static constexpr CbBypassCfg kCruiseGateCfg { .forceValue = 0u };

  ///  CRUISE toggle — enable/disable normal cruise mode (□ button).
  ///  bound=1 : when toggling "on", CRUISE_ACTIVE is set to 1 (active).
static constexpr CbBtnCfg kCruiseToggleCfg {
    .bound   = 1u,                          ///< "On" value — normal cruise active.
    .holdMs  = 0u,                          ///< Immediate press.
    .trigger = CbBtnTrigger::ON_PRESS,
};


// =============================================================================
// 2. STATE
// =============================================================================

static CbBtnState gCruiseToggleState {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Normal-cruise INPUT chain proc array.
 * @details Steps:
 *   1  gate   : SUBGEAR_BUS != 0 → claim value=0 (deactivate cruise in subgear).
 *   2  toggle : □ button — toggles CRUISE_ACTIVE 0 ↔ 1 (when gate does not claim).
 */
static CbProc kCruiseInputProcs[] = {
      // 1. gate — subgear active → force CRUISE_ACTIVE=0 and claim (skip toggle).
    { .name  = "gate",
      .inCh  = AnalogComBusID::SUBGEAR_BUS,
      .fn    = cb_bypass_fn,
      .cfg   = &kCruiseGateCfg,
    },
      // 2. toggle — □ button: 0 ↔ 1 (only runs when gate does not claim).
    { .name  = "cruise_toggle",
      .inCh  = DigitalComBusID::CRUISE_TOGGLE_BTN,
      .fn    = cb_btn_toggle_fn,
      .cfg   = &kCruiseToggleCfg,
      .state = &gCruiseToggleState,
    },
};

// EOF cruise_input_config.h
