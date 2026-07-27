/*!****************************************************************************
 * @file  subgear_config.h
 * @brief Dumper truck — INPUT subgear chain: cfg, state, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbBtnCfg, CbBtnState, CbBtnTrigger, CbProc, cb_btn_toggle_fn,
 *     cb_btn_inc_fn, cb_btn_dec_fn, cb_bypass_fn,
 *     AnalogComBusID, DigitalComBusID, kDumperTruckGearShift.
 *
 *   Chain function: SUBGEAR_BUS → [neutral-gate, toggle, inc, dec] → SUBGEAR_BUS.
 *   In:  SUBGEAR_BUS (current sub-gear index), DRIVE_STATE_BUS (gate signal).
 *   Out: SUBGEAR_BUS (updated sub-gear index).
 *
 *   1  neutral-gate: DRIVE_STATE_BUS != 0 → claim, value unchanged.
 *      Prevents toggling or incrementing sub-gear while the vehicle is moving.
 *   2  toggle: SHARE button — 0 ↔ 1 (enable/disable sub-gear mode).
 *   3  inc: L1 trigger — increment, upper bound = subGearCount.
 *   4  dec: L2 trigger — decrement, lower bound = 1.
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  SUBGEAR toggle — enable/disable sub-gear mode (SHARE button).
  ///  bound=1 : when toggling "on", SUBGEAR_BUS is set to 1 (first sub-gear).
static constexpr CbBtnCfg kSubGearToggleCfg {
    .bound   = 1u,                          ///< "On" value — first sub-gear index.
    .holdMs  = 0u,                          ///< Immediate press.
    .trigger = CbBtnTrigger::ON_PRESS,
};

  ///  SUBGEAR increment — up through sub-gears (L1 trigger).
  ///  bound = runtime subGearCount from the active GearShiftProfile.
static constexpr CbBtnCfg kSubGearIncCfg {
    .bound   = kDumperTruckGearShift->subGearCount,  ///< Upper limit (3 for kHeavy6 subgears).
    .holdMs  = 0u,                                   ///< Immediate press.
    .trigger = CbBtnTrigger::ON_PRESS,
};

  ///  SUBGEAR decrement — down through sub-gears (L2 trigger).
  ///  bound=1 : cannot decrement below first sub-gear index.
static constexpr CbBtnCfg kSubGearDecCfg {
    .bound   = 1u,                          ///< Lower limit.
    .holdMs  = 0u,                          ///< Immediate press.
    .trigger = CbBtnTrigger::ON_PRESS,
};


// =============================================================================
// 2. STATE
// =============================================================================

static CbBtnState gSubGearToggleState {};
static CbBtnState gSubGearIncState    {};
static CbBtnState gSubGearDecState    {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Subgear chain proc array.
 * @details Steps:
 *   1  neutral-gate : DRIVE_STATE_BUS != 0 → claim, value unchanged.
 *   2  toggle       : SHARE button — 0 ↔ 1 (enable/disable sub-gear mode).
 *   3  inc          : L1 trigger — increment, upper bound = subGearCount.
 *   4  dec          : L2 trigger — decrement, lower bound = 1.
 */
static CbProc kSubGearProcs[] = {
      // 1. neutral-gate — DRIVE_STATE_BUS != 0 → claim, value unchanged.
      //    Prevents abrupt gear/subgear transitions mid-drive.
    { .name = "neutral-gate",
      .inCh = AnalogComBusID::DRIVE_STATE_BUS,
      .fn   = cb_bypass_fn,
      // cfg = nullptr → value unchanged on claim (preserve current SUBGEAR_BUS).
    },
      // 2. toggle — SHARE button: 0 ↔ 1
    { .name  = "subgear_toggle",
      .inCh  = DigitalComBusID::SUBGEAR_SET_BTN,
      .fn    = cb_btn_toggle_fn,
      .cfg   = &kSubGearToggleCfg,
      .state = &gSubGearToggleState,
    },
      // 3. inc — L1 button: val++, max = subGearCount
    { .name  = "subgear_inc",
      .inCh  = DigitalComBusID::GEAR_UP_BTN,
      .fn    = cb_btn_inc_fn,
      .cfg   = &kSubGearIncCfg,
      .state = &gSubGearIncState,
    },
      // 4. dec — L2 button: val--, min = 1
    { .name  = "subgear_dec",
      .inCh  = DigitalComBusID::GEAR_DOWN_BTN,
      .fn    = cb_btn_dec_fn,
      .cfg   = &kSubGearDecCfg,
      .state = &gSubGearDecState,
    },
};

// EOF subgear_config.h
