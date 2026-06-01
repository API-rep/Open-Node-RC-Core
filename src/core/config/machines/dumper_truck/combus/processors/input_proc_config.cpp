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
 *     INPUT_KEY_RUNLEVEL : in(KEY_ACTIVE) -> key_on(ON_PRESS, 0ms) -> key_off(ON_PRESS, 3000ms)
 *                                         -> runlevel(KEY_ACTIVE) -> out(KEY_ACTIVE)
 *                          key_on fires on press-down; key_off fires WHILE held >= 3 s.
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
#include <core/system/combus/processors/input/cb_btn.h>  // cb_btn_push_fn, cb_btn_toggle_fn, cb_btn_inc_fn, cb_btn_dec_fn, CbBtnCfg, CbBtnState, CbBtnTrigger
#include <core/system/combus/processors/base/cb_runlevel.h>              // cb_runlevel_fn, CbRunlevelCfg, CbRunlevelState
#include <core/system/combus/processors/base/cb_bypass.h>              // cb_bypass_fn
using namespace DumperTruck;


// =============================================================================
// 1. BUTTON CONFIGS
// =============================================================================

//  SUBGEAR toggle — enable/disable sub-gear mode (SHARE button).
//  bound=1 : when toggling "on", SUBGEAR_BUS is set to 1 (first sub-gear).
static constexpr CbBtnCfg kSubGearToggleCfg {
    .bound   = 1u,                          ///< "On" value — first sub-gear index.
    .holdMs  = 0u,                          ///< Immediate press.
    .trigger = CbBtnTrigger::ON_PRESS,
};

//  SUBGEAR increment — up through sub-gears (L1 trigger).
//  bound = runtime subGearCount from the active GearShiftProfile.
static constexpr CbBtnCfg kSubGearIncCfg {
    .bound   = kDumperTruckGearShift->subGearCount,  ///< Upper limit (3 for kHeavy6 subgears).
    .holdMs  = 0u,                                   ///< Immediate press.
    .trigger = CbBtnTrigger::ON_PRESS,
};

//  SUBGEAR decrement — down through sub-gears (L2 trigger).
//  bound=1 : cannot decrement below first sub-gear index.
static constexpr CbBtnCfg kSubGearDecCfg {
    .bound   = 1u,                          ///< Lower limit.
    .holdMs  = 0u,                          ///< Immediate press.
    .trigger = CbBtnTrigger::ON_PRESS,
};

//  DIRECT_DRIVE toggle — enable/disable direct-drive mode (OPTIONS button).
//  bound=1 : when toggling "on", DIRECT_DRIVE is set to 1 (active).
static constexpr CbBtnCfg kDirectDriveToggleCfg {
    .bound   = 1u,                          ///< "On" value — direct-drive active.
    .holdMs  = 0u,                          ///< Immediate press.
    .trigger = CbBtnTrigger::ON_PRESS,
};


// =============================================================================
// 2. BUTTON STATES
// =============================================================================

static CbBtnState gSubGearToggleState    {};
static CbBtnState gSubGearIncState       {};
static CbBtnState gSubGearDecState       {};
static CbBtnState gDirectDriveToggleState {};

//  KEY button states — key_on: immediate ON; key_off: 3 s hold → OFF.
static CbBtnState gKeyOnState  {};
static CbBtnState gKeyOffState {};

//  KEY runlevel state — &comBus resolved at startup (pointer init is zero-init safe).
static CbRunlevelState gKeyRunlevelState {
    .bus       = &comBus,
    .prevValue = false,
};


// =============================================================================
// 3. PROCESSOR ARRAYS
// =============================================================================

static CbProc kSubGearProcs[] = {
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
};


// =============================================================================
// 4. CHANNEL OWNER
// =============================================================================

static constexpr ChanOwner kInputOwner = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM);


// =============================================================================
// 5. DIRECT DRIVE PROCESSOR ARRAY
// =============================================================================

static CbProc kDirectDriveProcs[] = {
    // toggle — OPTIONS button: 0 ↔ 1
    { .name    = "direct_drive_toggle",
      .inCh    = DigitalComBusID::DIRECT_DRIVE_BTN,
      .fn      = cb_btn_toggle_fn,
      .cfg     = &kDirectDriveToggleCfg,
      .state   = &gDirectDriveToggleState,
    },
};


// =============================================================================
// 6. KEY RUNLEVEL PROCESSOR ARRAY
// =============================================================================

//  key_on  — immediate ON_PRESS (holdMs=0): fires on rising edge → KEY_ACTIVE = 1.
static constexpr CbBtnCfg kKeyOnCfg {
    .bound   = 1u,
    .holdMs  = 0u,                    ///< Fires on press-down.
    .trigger = CbBtnTrigger::ON_PRESS,
};

//  key_off — long ON_PRESS (holdMs=3000): fires WHILE held >= 3 s → KEY_ACTIVE = 0.
static constexpr CbBtnCfg kKeyOffCfg {
    .bound   = 0u,
    .holdMs  = 3000u,                 ///< 3 s continuous hold → shutdown.
    .trigger = CbBtnTrigger::ON_PRESS,
};

//  RunLevel config — KEY_ACTIVE 0→1 → STARTING; 1→0 → TURNING_OFF.
static constexpr CbRunlevelCfg kKeyRunlevelCfg {
    .activeLevel  = RunLevel::STARTING,
    .defaultLevel = RunLevel::TURNING_OFF,
};

static CbProc kKeyRunlevelProcs[] = {
    // key_on — press-down → KEY_ACTIVE = 1.  Once per press (repeat-guarded).
    { .name  = "key_on",
      .inCh  = DigitalComBusID::KEY_BTN,
      .fn    = cb_btn_push_fn,
      .cfg   = &kKeyOnCfg,
      .state = &gKeyOnState,
    },
    // key_off — hold >= 3 s WHILE pressed → KEY_ACTIVE = 0.  Fires before release.
    { .name  = "key_off",
      .inCh  = DigitalComBusID::KEY_BTN,
      .fn    = cb_btn_push_fn,
      .cfg   = &kKeyOffCfg,
      .state = &gKeyOffState,
    },
    // runlevel — edge detection on KEY_ACTIVE → drives RunLevel.
    //   Skipped one frame when key_on/key_off claims; 1-frame (~10 ms) delay is imperceptible.
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
    .inCh       = AnalogComBusID::SUBGEAR_BUS,
    .outCh      = AnalogComBusID::SUBGEAR_BUS,
    .procs      = kSubGearProcs,
    .procCount  = static_cast<uint8_t>(std::size(kSubGearProcs)),
    .chainOwner = kInputOwner,
  },

  { .name       = "direct_drive",
    .inCh       = DigitalComBusID::DIRECT_DRIVE_BTN,
    .outCh      = DigitalComBusID::DIRECT_DRIVE,
    .procs      = kDirectDriveProcs,
    .procCount  = static_cast<uint8_t>(std::size(kDirectDriveProcs)),
    .chainOwner = kInputOwner,
  },

  { .name       = "key_runlevel",
    .inCh       = DigitalComBusID::KEY_ACTIVE,
    .outCh      = DigitalComBusID::KEY_ACTIVE,
    .procs      = kKeyRunlevelProcs,
    .procCount  = static_cast<uint8_t>(std::size(kKeyRunlevelProcs)),
    .chainOwner = kInputOwner,
  },

};

#endif  // IS_MAINBOARD

// EOF input_proc_config.cpp
