/*!****************************************************************************
 * @file  key_runlevel_config.h
 * @brief Dumper truck — INPUT key chain: cfg, state, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbBtnCfg, CbBtnState, CbBtnTrigger, CbProc, cb_btn_push_fn,
 *     DigitalComBusID, comBus.
 *
 *   Chain function: KEY_BTN → [key_on, key_off] → KEY_ACTIVE.
 *   In:  KEY_BTN (physical key switch).
 *   Out: KEY_ACTIVE (flag — 0 = engine off, 1 = engine on).
 *
 *   1  key_on:    immediate ON_PRESS (holdMs=0) → KEY_ACTIVE = 1 on rising edge.
 *   2  key_off:   long ON_PRESS (holdMs=3000) → KEY_ACTIVE = 0 while held >= 3 s.
 *
 *   The RunLevel FSM (KEY_ACTIVE → RUNLEVEL) is now handled by the dedicated
 *   runlevelProcs[] chain (see runlevel_config.h), NOT by this chain.
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  key_on — immediate ON_PRESS (holdMs=0): fires on rising edge → KEY_ACTIVE = 1.
static constexpr CbBtnCfg kKeyOnCfg {
    .bound   = 1u,
    .holdMs  = 0u,                    ///< Fires on press-down.
    .trigger = CbBtnTrigger::ON_PRESS,
};

  ///  key_off — long ON_PRESS (holdMs=3000): fires WHILE held >= 3 s → KEY_ACTIVE = 0.
static constexpr CbBtnCfg kKeyOffCfg {
    .bound   = 0u,
    .holdMs  = 3000u,                 ///< 3 s continuous hold → shutdown.
    .trigger = CbBtnTrigger::ON_PRESS,
};


// =============================================================================
// 2. STATE
// =============================================================================

  //  KEY button states — key_on: immediate ON; key_off: 3 s hold → OFF.
static CbBtnState gKeyOnState  {};
static CbBtnState gKeyOffState {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Key chain proc array.
 * @details Steps:
 *   1  key_on   : press-down → KEY_ACTIVE = 1.  Once per press (repeat-guarded).
 *   2  key_off  : hold >= 3 s WHILE pressed → KEY_ACTIVE = 0.  Fires before release.
 */
static CbProc kKeyRunlevelProcs[] = {
      // 1. key_on — press-down → KEY_ACTIVE = 1.  Once per press (repeat-guarded).
    { .name  = "key_on",
      .inCh  = DigitalComBusID::KEY_BTN,
      .fn    = cb_btn_push_fn,
      .cfg   = &kKeyOnCfg,
      .state = &gKeyOnState,
    },
      // 2. key_off — hold >= 3 s WHILE pressed → KEY_ACTIVE = 0.
    { .name  = "key_off",
      .inCh  = DigitalComBusID::KEY_BTN,
      .fn    = cb_btn_push_fn,
      .cfg   = &kKeyOffCfg,
      .state = &gKeyOffState,
    },
};

// EOF key_runlevel_config.h