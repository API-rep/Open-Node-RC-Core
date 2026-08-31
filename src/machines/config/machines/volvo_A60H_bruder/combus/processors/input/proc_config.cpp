/*!****************************************************************************
 * @file  proc_config.cpp
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
 *                          key_on fires on press-down; key_off fires WHILE held >= 3 s. *
 *     INPUT_CRUISE_NORMAL : in(CRUISE_ACTIVE) -> gate(SUBGEAR_BUS != 0 → force 0)
 *                                              -> toggle(CRUISE_TOGGLE_BTN)
 *                                            → out(CRUISE_ACTIVE)
 *                          gate forces CRUISE_ACTIVE=0 while in subgear/crawler mode. *
 *   Read-modify-write pattern: each proc reads current value, modifies it,
 *   passes to next proc.  Last proc (cb_out_fn) commits to the output channel.
 *******************************************************************************
 */

#ifdef IS_MAINBOARD

#include "proc_config.h"

#include <machines/config/machines/volvo_A60H_bruder/combus/combus.h>       // AnalogComBusID, DigitalComBusID, comBus
#include <core/config/machines/dumper_truck/motion/dumper_truck_motion.h>  // kDumperTruckGearShift
#include <core/system/combus/combus_defs.h>                                          // ChanLayer
#include <core/system/combus/processors/input/cb_btn.h>    // cb_btn_push_fn, cb_btn_toggle_fn, cb_btn_inc_fn, cb_btn_dec_fn, CbBtnCfg, CbBtnState, CbBtnTrigger
#include <core/system/combus/processors/base/cb_runlevel.h>               // cb_runlevel_fn, CbRunlevelCfg, CbRunlevelState
#include <core/system/combus/processors/base/cb_bypass.h>                 // cb_bypass_fn


// Chain configs — included in any order (no cross-dependencies between input chains).
// All types and function pointers are resolved by the common includes above.
#include "subgear_config.h"        // kSubGearProcs — neutral-gate + toggle + inc + dec
#include "direct_drive_config.h"   // kDirectDriveProcs — toggle
#include "key_runlevel_config.h"   // kKeyRunlevelProcs — key_on + key_off + runlevel
#include "cruise_input_config.h"   // kCruiseInputProcs — gate(SUBGEAR) + toggle(□)


// =============================================================================
// =============================================================================
// 1. CHAIN ARRAY
// =============================================================================

CbChain kInputChains[INPUT_CH_COUNT] = {

  { .name       = "subgear",
    .inCh       = AnalogComBusID::SUBGEAR_BUS,
    .outCh      = AnalogComBusID::SUBGEAR_BUS,
    .procs      = kSubGearProcs,
    .procCount  = static_cast<uint8_t>(std::size(kSubGearProcs)),
  },

  { .name       = "direct_drive",
    .inCh       = DigitalComBusID::DIRECT_DRIVE,      // current mode state — seeded into value for toggle
    .outCh      = DigitalComBusID::DIRECT_DRIVE,
    .procs      = kDirectDriveProcs,
    .procCount  = static_cast<uint8_t>(std::size(kDirectDriveProcs)),
  },

  { .name       = "key_runlevel",
    .inCh       = DigitalComBusID::KEY_ACTIVE,
    .outCh      = DigitalComBusID::KEY_ACTIVE,
    .procs      = kKeyRunlevelProcs,
    .procCount  = static_cast<uint8_t>(std::size(kKeyRunlevelProcs)),
  },

  { .name       = "cruise_normal",
    .inCh       = DigitalComBusID::CRUISE_ACTIVE,   // seeds current toggle state into value
    .outCh      = DigitalComBusID::CRUISE_ACTIVE,
    .procs      = kCruiseInputProcs,
    .procCount  = static_cast<uint8_t>(std::size(kCruiseInputProcs)),
  },

  // INPUT_THROTTLE — passthrough: THROTTLE_STICK (raw bipolar) → THROTTLE_BUS.
  // No procs yet — conditioning (center+abs+scale+dir) added in the next migration step.
  { .name       = "throttle",
    .inCh       = AnalogComBusID::THROTTLE_STICK,
    .outCh      = AnalogComBusID::THROTTLE_BUS,
    .procs      = nullptr,
    .procCount  = 0u,
  },

};

#endif  // IS_MAINBOARD

// EOF proc_config.cpp
