/*!****************************************************************************
 * @file  input_proc_config.cpp
 * @brief Dumper truck — INPUT chain configuration.
 *
 * @details Defines the INPUT CbChain array for the dumper-truck machine class.
 *   Compiled only in machine-node builds (IS_MAINBOARD guard).
 *
 *   INPUT pipelines:
 *     INPUT_SUBGEAR      : in(SUBGEAR_BUS) → neutral-gate(DRIVE_STATE_BUS),
 *                                              toggle(SUBGEAR_SET_BTN, bound=1),
 *                                              inc(GEAR_UP_BTN, bound=subGearCount),
 *                                              dec(GEAR_DOWN_BTN, bound=1)
 *                                            → out(SUBGEAR_BUS)
 *                          neutral-gate blocks toggle/inc/dec when driving
 *                          (DRIVE_STATE_BUS != 0 → claim, value unchanged).
 *     INPUT_DIRECT_DRIVE : in(DIRECT_DRIVE_BTN) → toggle(bound=1) → out(DIRECT_DRIVE)
 *     INPUT_KEY_RUNLEVEL : in(KEY_BTN) → latch(holdMs=3000) → out(KEY_ACTIVE)
 *                                       → runlevel(KEY_ACTIVE → STARTING / TURNING_OFF)
 *
 *   Read-modify-write pattern: each proc reads current value, modifies it,
 *   passes to next proc.  Last proc (cb_out_fn) commits to the output channel.
 *******************************************************************************
 */

#ifdef IS_MAINBOARD

#include "input_proc_config.h"

#include <core/config/machines/dumper_truck/combus/combus.h>           // AnalogComBusID, DigitalComBusID, comBus
#include <core/config/machines/dumper_truck/motion/dumper_truck_motion.h>  // kDumperTruckGearShift
#include <struct/combus_struct.h>                                      // makeChanOwner, ComBusOwner
#include <core/system/combus/processors/input/cb_btn.h>                // cb_btn_toggle_fn, cb_btn_inc_fn, cb_btn_dec_fn, CbBtnCfg, CbBtnState
#include <core/system/combus/processors/input/cb_btn_latch.h>           // cb_btn_latch_fn, CbBtnLatchCfg, CbBtnLatchState
#include <core/system/combus/processors/base/cb_runlevel.h>              // cb_runlevel_fn, CbRunlevelCfg, CbRunlevelState
#include <core/system/combus/processors/base/cb_io.h>                  // cb_in_fn, cb_out_fn
#include <core/system/combus/processors/base/cb_bypass.h>              // cb_bypass_fn
using namespace DumperTruck;


// =============================================================================
// 1. BUTTON CONFIGS
// =============================================================================

//  SUBGEAR toggle — enable/disable sub-gear mode (SHARE button).
//  bound=1 : when toggling "on", SUBGEAR_BUS is set to 1 (first sub-gear).
static constexpr CbBtnCfg kSubGearToggleCfg {
    .bound      = 1u,     ///< "On" value — first sub-gear index.
    .debounceMs = 50u,    ///< 50 ms debounce.
    .holdMs     = 0u,     ///< Rising-edge trigger (no long-press).
};

//  SUBGEAR increment — up through sub-gears (L1 trigger).
//  bound = runtime subGearCount from the active GearShiftProfile.
static constexpr CbBtnCfg kSubGearIncCfg {
    .bound      = kDumperTruckGearShift->subGearCount,  ///< Upper limit (3 for kHeavy3).
    .debounceMs = 50u,                                  ///< 50 ms debounce.
    .holdMs     = 0u,                                   ///< Rising-edge trigger.
};

//  SUBGEAR decrement — down through sub-gears (L2 trigger).
//  bound=1 : cannot decrement below first sub-gear index.
static constexpr CbBtnCfg kSubGearDecCfg {
    .bound      = 1u,     ///< Lower limit.
    .debounceMs = 50u,    ///< 50 ms debounce.
    .holdMs     = 0u,     ///< Rising-edge trigger.
};

//  DIRECT_DRIVE toggle — enable/disable direct-drive mode (OPTIONS button).
//  bound=1 : when toggling "on", DIRECT_DRIVE is set to 1 (active).
static constexpr CbBtnCfg kDirectDriveToggleCfg {
    .bound      = 1u,     ///< "On" value — direct-drive active.
    .debounceMs = 50u,    ///< 50 ms debounce.
    .holdMs     = 0u,     ///< Rising-edge trigger (no long-press).
};


// =============================================================================
// 2. BUTTON STATES
// =============================================================================

static CbBtnState gSubGearToggleState    {};
static CbBtnState gSubGearIncState       {};
static CbBtnState gSubGearDecState       {};
static CbBtnState gDirectDriveToggleState {};

//  KEY latch state (non-constexpr: runtime init only).
static CbBtnLatchState gKeyLatchState {};

//  KEY runlevel state — &comBus resolved at startup (pointer init is zero-init safe).
static CbRunlevelState gKeyRunlevelState {
    .bus       = &comBus,
    .prevValue = false,
};


// =============================================================================
// 3. PROCESSOR ARRAYS
// =============================================================================

static CbProc kSubGearProcs[] = {
    // in — seed pipeline from current SUBGEAR_BUS value (read-modify-write).
    { .name  = "in",
      .inCh  = AnalogComBusID::SUBGEAR_BUS,
      .fn    = cb_in_fn,
    },
    // neutral-gate — block subgear changes while the vehicle is moving.
    //   DRIVE_STATE_BUS != 0 (FWD / REV / BRAKING) → claim, value unchanged.
    //   DRIVE_STATE_BUS == 0 (STANDING) → no action, toggle/inc/dec proceed.
    //   Prevents abrupt gear/subgear transitions mid-drive.
    { .name  = "neutral-gate",
      .inCh  = AnalogComBusID::DRIVE_STATE_BUS,
      .fn    = cb_bypass_fn,
      // cfg = nullptr → value unchanged on claim (preserve current SUBGEAR_BUS).
    },
    // toggle — SHARE button: 0 ↔ 1
    { .name    = "subgear_toggle",
      .inCh    = DigitalComBusID::SUBGEAR_SET_BTN,
      .fn      = cb_btn_toggle_fn,
      .cfg     = &kSubGearToggleCfg,
      .state   = &gSubGearToggleState,
    },
    // inc — L1 button: val++, max=subGearCount
    { .name    = "subgear_inc",
      .inCh    = DigitalComBusID::GEAR_UP_BTN,
      .fn      = cb_btn_inc_fn,
      .cfg     = &kSubGearIncCfg,
      .state   = &gSubGearIncState,
    },
    // dec — L2 button: val--, min=1
    { .name    = "subgear_dec",
      .inCh    = DigitalComBusID::GEAR_DOWN_BTN,
      .fn      = cb_btn_dec_fn,
      .cfg     = &kSubGearDecCfg,
      .state   = &gSubGearDecState,
    },
    // out — commit modified value back to SUBGEAR_BUS.
    { .name  = "out",
      .outCh = AnalogComBusID::SUBGEAR_BUS,
      .fn    = cb_out_fn,
    },
};


// =============================================================================
// 4. CHANNEL OWNER
// =============================================================================

static constexpr ChanOwner kInputOwner = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM);


// =============================================================================
// 5. DIRECT DRIVE PROCESSOR ARRAY
// =============================================================================

static CbProc kDirectDriveProcs[] = {
    // in — seed pipeline from DIRECT_DRIVE_BTN state.
    { .name  = "in",
      .inCh  = DigitalComBusID::DIRECT_DRIVE_BTN,
      .fn    = cb_in_fn,
    },
    // toggle — OPTIONS button: 0 ↔ 1
    { .name    = "direct_drive_toggle",
      .inCh    = DigitalComBusID::DIRECT_DRIVE_BTN,
      .fn      = cb_btn_toggle_fn,
      .cfg     = &kDirectDriveToggleCfg,
      .state   = &gDirectDriveToggleState,
    },
    // out — commit result to DIRECT_DRIVE.
    { .name  = "out",
      .outCh = DigitalComBusID::DIRECT_DRIVE,
      .fn    = cb_out_fn,
    },
};


// =============================================================================
// 6. KEY RUNLEVEL PROCESSOR ARRAY
// =============================================================================

//  Latch config — short press to activate; 3 s hold to deactivate.
static constexpr CbBtnLatchCfg kKeyLatchCfg {
    .holdMs = 3000u,  ///< 3 s continuous hold clears the latch.
};

//  RunLevel config — latch ON → STARTING; latch OFF → TURNING_OFF (graceful shutdown).
static constexpr CbRunlevelCfg kKeyRunlevelCfg {
    .activeLevel  = RunLevel::STARTING,     ///< Set when KEY_ACTIVE goes 0→1.
    .defaultLevel = RunLevel::TURNING_OFF,  ///< Set when KEY_ACTIVE goes 1→0 — FSM drives IDLE from there.
};

static CbProc kKeyRunlevelProcs[] = {
    // in — seed pipeline from raw KEY_BTN state.
    { .name  = "in",
      .inCh  = DigitalComBusID::KEY_BTN,
      .fn    = cb_in_fn,
    },
    // latch — rising edge → ON; hold 3 s → OFF.
    { .name  = "key_latch",
      .inCh  = DigitalComBusID::KEY_BTN,
      .fn    = cb_btn_latch_fn,
      .cfg   = &kKeyLatchCfg,
      .state = &gKeyLatchState,
    },
    // out — commit latch state to KEY_ACTIVE wire channel.
    { .name  = "out",
      .outCh = DigitalComBusID::KEY_ACTIVE,
      .fn    = cb_out_fn,
    },
    // runlevel — KEY_ACTIVE transitions drive RunLevel changes.
    { .name  = "runlevel",
      .inCh  = DigitalComBusID::KEY_ACTIVE,
      .fn    = cb_runlevel_fn,
      .cfg   = &kKeyRunlevelCfg,
      .state = &gKeyRunlevelState,
    },
};


// =============================================================================
// 7. CHAIN ARRAY
// =============================================================================

CbChain kInputChains[INPUT_CH_COUNT] = {

  { .name       = "subgear",
    .procs      = kSubGearProcs,
    .procCount  = static_cast<uint8_t>(std::size(kSubGearProcs)),
    .chainOwner = kInputOwner,
  },

  { .name       = "direct_drive",
    .procs      = kDirectDriveProcs,
    .procCount  = static_cast<uint8_t>(std::size(kDirectDriveProcs)),
    .chainOwner = kInputOwner,
  },

  { .name       = "key_runlevel",
    .procs      = kKeyRunlevelProcs,
    .procCount  = static_cast<uint8_t>(std::size(kKeyRunlevelProcs)),
    .chainOwner = kInputOwner,
  },

};

#endif  // IS_MAINBOARD

// EOF input_proc_config.cpp
