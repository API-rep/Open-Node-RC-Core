/*!****************************************************************************
 * @file  input_proc_config.cpp
 * @brief Dumper truck — INPUT chain configuration.
 *
 * @details Defines the INPUT CbChain array for the dumper-truck machine class.
 *   Compiled only in machine-node builds (IS_MAINBOARD guard).
 *
 *   INPUT pipelines:
 *     INPUT_SUBGEAR      : in(SUBGEAR_BUS) → toggle(SUBGEAR_SET_BTN, bound=1),
 *                                              inc(GEAR_UP_BTN, bound=subGearCount),
 *                                              dec(GEAR_DOWN_BTN, bound=1)
 *                                            → out(SUBGEAR_BUS)
 *     INPUT_DIRECT_DRIVE : in(DIRECT_DRIVE_BTN) → toggle(bound=1) → out(DIRECT_DRIVE)
 *
 *   Read-modify-write pattern: each proc reads current value, modifies it,
 *   passes to next proc.  Last proc (cb_out_fn) commits to the output channel.
 *******************************************************************************
 */

#ifdef IS_MAINBOARD

#include "input_proc_config.h"

#include <core/config/machines/dumper_truck/combus/combus.h>           // AnalogComBusID, DigitalComBusID
#include <core/config/machines/dumper_truck/motion/dumper_truck_motion.h>  // kDumperTruckGearShift
#include <struct/combus_struct.h>                                      // makeChanOwner, ComBusOwner
#include <core/system/combus/processors/input/cb_btn.h>                // cb_btn_toggle_fn, cb_btn_inc_fn, cb_btn_dec_fn, CbBtnCfg, CbBtnState
#include <core/system/combus/processors/base/cb_io.h>                  // cb_in_fn, cb_out_fn
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


// =============================================================================
// 3. PROCESSOR ARRAYS
// =============================================================================

static CbProc kSubGearProcs[] = {
    // in — seed pipeline from current SUBGEAR_BUS value (read-modify-write).
    { .name  = "in",
      .inCh  = AnalogComBusID::SUBGEAR_BUS,
      .fn    = cb_in_fn,
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
// 5. CHANNEL ARRAY
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
// 5. CHANNEL ARRAY
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

};

#endif  // IS_MAINBOARD

// EOF input_proc_config.cpp
