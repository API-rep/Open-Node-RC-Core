/*!****************************************************************************
 * @file  sim_proc_config.cpp
 * @brief Dumper truck — CbChain pipeline configuration (SIM layer).
 *
 * @details Defines the SIM CbChain array for the dumper-truck machine class.
 *   Compiled only in machine-node builds (IS_MAINBOARD guard).
 *
 *   Channel pipelines (optInCh → procs → outCh):
 *     SIM_THROTTLE : THROTTLE_BUS → ramp, drive-state(→DRIVE_STATE_BUS),
 *                    center, abs, scale, bypass(DIRECT_DRIVE),
 *                    ratio(DIRECT_DRIVE, GEAR) → RPM_BUS
 *     SIM_GEAR     : RPM_BUS → bypass(DIRECT_DRIVE),
 *                    gear-fsm(DRIVE_STATE_BUS, SUBGEAR_BUS),
 *                    gear-ramp(SUBGEAR_BUS) → GEAR
 *     SIM_TRACTION : RPM_BUS → rpm_to_speed(DRIVE_STATE_BUS, GEAR) → ESC_SPEED_BUS
 *     SIM_STEERING : STEERING_BUS → bypass(DIRECT_DRIVE), ramp → STEERING_RAMPED_BUS
 *     SIM_DUMP     : DUMP_BUS     → bypass(DIRECT_DRIVE), ramp → DUMP_RAMPED_BUS
 *
 *   SUBGEAR_BUS is written by INPUT chain (cb_btn procs) — see input_proc_config.cpp.
 *******************************************************************************
 */

#ifdef IS_MAINBOARD

#include "sim_proc_config.h"

#include <core/config/machines/dumper_truck/combus/combus.h>  // AnalogComBusID, DigitalComBusID
#include <core/config/hw/simulation_presets.h>    // kHeavy3_steps, kGearShift_Heavy3Speed
#include <struct/combus_struct.h>                 // makeChanOwner, ComBusOwner
#include <core/system/combus/combus_res.h>        // CbusNeutral, pctToCbus
#include <core/system/combus/processors/motion/cb_ramp.h>  // cb_ramp_fn, CbRampCfg, CbRampState
#include <core/system/combus/processors/base/cb_bypass.h>  // cb_bypass_fn
#include <core/system/combus/processors/math/cb_center.h>          // cb_center_fn, CbCenterCfg
#include <core/system/combus/processors/math/cb_abs.h>              // cb_abs_fn
#include <core/system/combus/processors/math/cb_scale.h>            // cb_scale_fn, CbScaleCfg
#include <core/system/combus/processors/motion/cb_dir.h>           // cb_dir_fn, CbDirCfg
#include <core/system/simulation/sim_gear.h>      // sim_gear_fn, sim_apply_ratio_fn, sim_gear_bypass_fn, sim_rpm_to_speed_fn, sim_gear_ramp_fn
using namespace DumperTruck;


// =============================================================================
// 1. RAMP CONFIGURATIONS
// =============================================================================

//  Steering — progressive start (5 %/tick @ 20 ms), instant stop.
static constexpr CbRampCfg kSteerAsymRamp {
    .rampTimeMs  = 20u,
    .accelSteps  = pctToCbus(5),
    .brakeSteps  = CbusNeutral,
    .neutralBand = 0u,
};

//  Dump body — slow raise, fast lower, instant stop.
static constexpr CbRampCfg kDumpAsymRamp {
    .rampTimeMs     = 20u,
    .accelSteps     = pctToCbus(2),
    .accelDownSteps = pctToCbus(4),
    .brakeSteps     = CbusNeutral,
    .neutralBand    = 0u,
};

//  Traction — heavy truck inertia.
static constexpr CbRampCfg kTractionRamp {
    .rampTimeMs  = 50u,
    .accelSteps  = pctToCbus(3),
    .brakeSteps  = pctToCbus(6),
    .neutralBand = 0u,
};

//  Traction ramp — RAM copy, mutated by sim_gear_ramp_fn on each gear change.
static CbRampCfg gTractionRampDyn = kTractionRamp;


// =============================================================================
// 2. MATH + GEAR CONFIGS
// =============================================================================

static constexpr CbCenterCfg kThrottleCenter {};
static constexpr CbDirCfg kThrottleDirCfg {};

static constexpr CbScaleCfg kThrottleScale {
    .inMax  = CbusNeutral,
    .outMax = static_cast<uint16_t>(kHeavy3_steps[std::size(kHeavy3_steps) - 1u].upShift),
};

static constexpr GearProcCfg kGearCfg {
    .profile = &kGearShift_Heavy3Speed,
};


// =============================================================================
// 3. RUNTIME STATES
// =============================================================================

static CbRampState        gSteerRampState          {};
static CbRampState        gDumpRampState           {};
static CbRampState        gTractionRampState       {};
static GearFsmState        gGearFsmState            {};
static ShiftDeltaState     gThrottleShiftDeltaState {};


// =============================================================================
// 4. PROCESSOR ARRAYS
// =============================================================================

static CbProc kThrottleProcs[] = {
    // ramp — heavy-truck inertia on throttle input.
    { .name    = "ramp",
      .fn      = cb_ramp_fn,
      .cfg     = &kTractionRamp,
      .dynCfg  = &gTractionRampDyn,
      .state   = &gTractionRampState,
    },
    // dir — side-effect: encodes direction → DRIVE_STATE_BUS.
    { .name      = "dir",
      .outCh  = AnalogComBusID::DRIVE_STATE_BUS,
      .fn        = cb_dir_fn,
      .cfg       = &kThrottleDirCfg,
    },
    // center — signed deviation from CbusNeutral.
    { .name  = "center",
      .fn    = cb_center_fn,
      .cfg   = &kThrottleCenter,
    },
    // abs — magnitude; sign side effect is unused here (direction tracked above).
    { .name  = "abs",
      .fn    = cb_abs_fn,
    },
    // scale — RPM magnitude in [0..maxRpm].
    { .name  = "scale",
      .fn    = cb_scale_fn,
      .cfg   = &kThrottleScale,
    },
    // bypass — DIRECT_DRIVE HIGH → claim (raw magnitude bypasses ratio, flows to RPM_BUS).
    { .name      = "bypass",
      .inCh   = { DigitalComBusID::DIRECT_DRIVE },
      .fn        = cb_bypass_fn,
    },
    // ratio — subtract shiftDelta RPM on upshift.
    { .name      = "ratio",
      .inCh   = { AnalogComBusID::GEAR },
      .fn        = sim_apply_ratio_fn,
      .cfg       = &kGearCfg,
      .state     = &gThrottleShiftDeltaState,
    },
};

static CbProc kGearProcs[] = {
    // bypass — DIRECT_DRIVE HIGH → set gear=1, claim (skips gear-fsm).
    { .name      = "bypass",
      .inCh   = { DigitalComBusID::DIRECT_DRIVE },
      .fn        = sim_gear_bypass_fn,
    },
    // gear-fsm — RPM → gear FSM; reads DRIVE_STATE_BUS + SUBGEAR_BUS.
    { .name      = "gear-fsm",
      .inCh   = { AnalogComBusID::DRIVE_STATE_BUS, AnalogComBusID::SUBGEAR_BUS },
      .fn        = sim_gear_fn,
      .cfg       = &kGearCfg,
      .state     = &gGearFsmState,
    },
    // gear-ramp — updates traction ramp dynCfg for new gear; reads SUBGEAR_BUS.
    { .name      = "gear-ramp",
      .inCh   = { AnalogComBusID::SUBGEAR_BUS },
      .fn        = sim_gear_ramp_fn,
      .cfg       = &kGearCfg,
      .dynCfg    = &gTractionRampDyn,
    },
};

static CbProc kTractionProcs[] = {
    // rpm_to_speed — RPM + gear → ESC_SPEED_BUS bipolar.
    { .name      = "rpm_to_speed",
      .inCh   = { AnalogComBusID::DRIVE_STATE_BUS, AnalogComBusID::GEAR },
      .fn        = sim_rpm_to_speed_fn,
      .cfg       = &kGearCfg,
    },
};

static CbProc kSteeringProcs[] = {
    // bypass — DIRECT_DRIVE HIGH → claim (raw steering bypasses ramp).
    { .name    = "bypass",
      .inCh = { DigitalComBusID::DIRECT_DRIVE },
      .fn      = cb_bypass_fn,
    },
    // ramp — progressive inertia.
    { .name  = "ramp",
      .fn    = cb_ramp_fn,
      .cfg   = &kSteerAsymRamp,
      .state = &gSteerRampState,
    },
};

static CbProc kDumpProcs[] = {
    // bypass — DIRECT_DRIVE HIGH → claim (raw dump bypasses ramp).
    { .name    = "bypass",
      .inCh = { DigitalComBusID::DIRECT_DRIVE },
      .fn      = cb_bypass_fn,
    },
    // ramp — progressive asymmetric inertia.
    { .name  = "ramp",
      .fn    = cb_ramp_fn,
      .cfg   = &kDumpAsymRamp,
      .state = &gDumpRampState,
    },
};


// =============================================================================
// 5. CHANNEL ARRAY
// =============================================================================

static constexpr ChanOwner kSimOwner = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM);

CbChain kSimChannels[SIM_CH_COUNT] = {

  { .name       = "throttle",
    .optInCh    = AnalogComBusID::THROTTLE_BUS,
    .outCh   = AnalogComBusID::RPM_BUS,
    .procs      = kThrottleProcs,
    .procCount  = static_cast<uint8_t>(std::size(kThrottleProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "gear",
    .optInCh    = AnalogComBusID::RPM_BUS,
    .outCh   = AnalogComBusID::GEAR,
    .procs      = kGearProcs,
    .procCount  = static_cast<uint8_t>(std::size(kGearProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "traction",
    .optInCh    = AnalogComBusID::RPM_BUS,
    .outCh   = AnalogComBusID::ESC_SPEED_BUS,
    .procs      = kTractionProcs,
    .procCount  = static_cast<uint8_t>(std::size(kTractionProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "steering",
    .optInCh    = AnalogComBusID::STEERING_BUS,
    .outCh   = AnalogComBusID::STEERING_RAMPED_BUS,
    .procs      = kSteeringProcs,
    .procCount  = static_cast<uint8_t>(std::size(kSteeringProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "dump",
    .optInCh    = AnalogComBusID::DUMP_BUS,
    .outCh   = AnalogComBusID::DUMP_RAMPED_BUS,
    .procs      = kDumpProcs,
    .procCount  = static_cast<uint8_t>(std::size(kDumpProcs)),
    .chainOwner = kSimOwner,
  },
};

#endif  // IS_MAINBOARD

// EOF sim_proc_config.cpp
