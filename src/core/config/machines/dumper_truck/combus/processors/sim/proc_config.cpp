/*!****************************************************************************
 * @file  proc_config.cpp
 * @brief Dumper truck — CbChain pipeline configuration (SIM layer).
 *
 * @details Defines the SIM CbChain array for the dumper-truck machine class.
 *   Compiled only in machine-node builds (IS_MAINBOARD guard).
 *
 *   Channel pipelines (in -> procs -> out), execution order:
 *     SIM_THROTTLE : in(THROTTLE_BUS) -> ramp, dir(->DRIVE_STATE_BUS),
 *                    b-in(BRAKE_BUS->state), brake(THROTTLE_BUS->BRAKE_BUS merged+ramp),
 *                    center, abs, scale, bypass(DIRECT_DRIVE) -> out(ESC_RPM_BUS)
 *                    ESC_RPM_BUS carries wheel_speed in RPM domain after THROTTLE.
 *
 *     SIM_TRACTION : in(ESC_RPM_BUS) -> upshift-damp(GEAR), subgear-cap(SUBGEAR_BUS),
 *                    gear-dir(DRIVE_STATE_BUS) -> out(ESC_SPEED_BUS)
 *                    Runs BEFORE GEAR so it reads wheel_speed from ESC_RPM_BUS
 *                    before gear_ratio_inv_fn overwrites it with engine_rpm.
 *
 *     SIM_GEAR     : in(ESC_RPM_BUS) -> gear-inv-ratio(->ESC_RPM_BUS side-effect),
 *                    subgear-claim(SUBGEAR_BUS), direct-claim(DIRECT_DRIVE),
 *                    gear-fsm(DRIVE_STATE_BUS), gear-ramp -> out(GEAR)
 *                    gear-inv-ratio: wheel_speed * 1000 / gearRatio[prevGear] = engine_rpm;
 *                    writes engine_rpm to ESC_RPM_BUS (sound node reads engine RPM).
 *
 *     SIM_STEERING : in(STEERING_BUS) -> bypass(DIRECT_DRIVE), ramp -> out(STEERING_RAMPED_BUS)
 *     SIM_DUMP     : in(DUMP_BUS) -> bypass(DIRECT_DRIVE), ramp -> out(DUMP_RAMPED_BUS)
 *
 *   SUBGEAR_BUS is written by INPUT chain (cb_btn procs) — see input_proc_config.cpp.
 *******************************************************************************
 */

#ifdef IS_MAINBOARD

#include "proc_config.h"

#include <core/config/machines/dumper_truck/combus/combus.h>  // AnalogComBusID, DigitalComBusID
#include <core/config/hw/simulation_presets.h>    // kVolvoD16J_steps, kGearShift_VolvoD16J
#include <struct/combus_struct.h>                 // makeChanOwner, ComBusOwner
#include <core/system/combus/combus_res.h>        // CbusNeutral, pctToCbus
#include <core/system/combus/processors/motion/cb_ramp.h>  // cb_sym_ramp_fn, CbRampCfg, CbRampState
#include <core/system/combus/processors/base/cb_bypass.h>  // cb_bypass_fn
#include <core/system/combus/processors/math/cb_center.h>          // cb_center_fn, CbCenterCfg
#include <core/system/combus/processors/math/cb_abs.h>              // cb_abs_fn
#include <core/system/combus/processors/math/cb_scale.h>            // cb_scale_fn, CbScaleCfg
#include <core/system/combus/processors/motion/cb_dir.h>           // cb_dir_fn, CbDirCfg
#include <core/system/combus/processors/motion/cb_brake.h>         // cb_brake_fn, cb_rev_brake_fn
#include <core/system/combus/processors/motion/cb_cruise.h>         // cb_cruise_fn, CbCruiseCfg, CbCruiseState
#include <core/system/combus/processors/modules/gear/cb_gear.h>    // gear_fsm_fn, gear_ratio_inv_fn, gear_ratio_fn, gear_subgear_cap_fn, gear_dir_fn, gear_dyn_ramp_fn, gear_upshift_damp_fn
using namespace DumperTruck;

// Chain configs — included in declaration order (throttle before gear, gear before traction).
// All types and function pointers are resolved by the common includes above.
#include "throttle_config.h"  // gTractionRampDyn — must precede gear (gear-ramp refs it)
#include "gear_config.h"      // kGearCfg, kGearProcs — must precede traction
#include "traction_config.h"  // kTractionProcs — refs kGearCfg
#include "steering_config.h"  // kSteeringProcs
#include "dump_config.h"      // kDumpProcs


// =============================================================================
// 1. CHANNEL ARRAY
// =============================================================================

static constexpr ChanOwner kSimOwner = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM);

CbChain kSimChannels[SIM_CH_COUNT] = {

  { .name       = "throttle",
    .inCh       = AnalogComBusID::THROTTLE_BUS,
    .outCh      = AnalogComBusID::ESC_RPM_BUS,
    .procs      = kThrottleProcs,
    .procCount  = static_cast<uint8_t>(std::size(kThrottleProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "traction",
    .inCh       = AnalogComBusID::ESC_RPM_BUS,
    .outCh      = AnalogComBusID::ESC_SPEED_BUS,
    .procs      = kTractionProcs,
    .procCount  = static_cast<uint8_t>(std::size(kTractionProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "gear",
    .inCh       = AnalogComBusID::ESC_RPM_BUS,
    .outCh      = AnalogComBusID::GEAR,
    .procs      = kGearProcs,
    .procCount  = static_cast<uint8_t>(std::size(kGearProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "steering",
    .inCh       = AnalogComBusID::STEERING_BUS,
    .outCh      = AnalogComBusID::STEERING_RAMPED_BUS,
    .procs      = kSteeringProcs,
    .procCount  = static_cast<uint8_t>(std::size(kSteeringProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "dump",
    .inCh       = AnalogComBusID::DUMP_BUS,
    .outCh      = AnalogComBusID::DUMP_RAMPED_BUS,
    .procs      = kDumpProcs,
    .procCount  = static_cast<uint8_t>(std::size(kDumpProcs)),
    .chainOwner = kSimOwner,
  },
};

#endif  // IS_MAINBOARD

// EOF proc_config.cpp
