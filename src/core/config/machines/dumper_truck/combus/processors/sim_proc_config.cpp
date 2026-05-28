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
 *     SIM_GEAR     : RPM_BUS → subgear-claim(SUBGEAR_BUS),
 *                    manual-claim(MANUAL_GEAR_SET), direct-claim(DIRECT_DRIVE),
 *                    gear-fsm(DRIVE_STATE_BUS), gear-ramp → GEAR
 *                    NOTE: subgear-claim forces GEAR=0 when sub-gear active.
 *     SIM_TRACTION : RPM_BUS → rpm_to_speed(DRIVE_STATE_BUS, GEAR),
 *                    subgear-speed(SUBGEAR_BUS, DRIVE_STATE_BUS, RPM_BUS) → ESC_SPEED_BUS
 *                    NOTE: rpm_to_speed outputs CbusNeutral when GEAR=0;
 *                    subgear-speed overrides with capped speed when SUBGEAR_BUS != 0.
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
#include <core/system/combus/processors/modules/gear/cb_gear.h>    // gear_fsm_fn, gear_upshift_drop_fn, gear_rpm_to_speed_fn, gear_dyn_ramp_fn, gear_subgear_speed_fn
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
    .neutralBand = pctToCbus(3),   ///< 1% deadzone — absorbs PS4 stick noise (±~0.4%) at neutral,
                                   ///<   preventing cb_dir_fn from oscillating FWD↔REV at rest.
};

//  Traction ramp — RAM copy, mutated by gear_dyn_ramp_fn on each gear change.
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

static constexpr CbBypassCfg kSubGearClaimBypass {
    .forceValue = 0u,  ///< Force GEAR = 0 (sub-gear sentinel) when sub-gear mode active.
                       ///< gear_rpm_to_speed_fn outputs CbusNeutral for GEAR=0;
                       ///< subgear-speed proc in TRACTION chain handles actual wheel speed.
};

static constexpr CbBypassCfg kDirectDriveGearBypass {
    .forceValue = 1u,  // Force GEAR = 1 on DIRECT_DRIVE claim.
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
    // ramp — heavy-truck inertia on throttle input (uses gTractionRampDyn).
    //   rampTimeMs is updated by gear-ramp proc in GEAR chain (1 cycle latency).
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
      .fn        = gear_upshift_drop_fn,
      .cfg       = &kGearCfg,
      .state     = &gThrottleShiftDeltaState,
    },
};

static CbProc kGearProcs[] = {
    // =========================================================================
    // CLAIM CASCADE — first claim wins, later procs skipped.
    // Priority: SubGear > Manual > Direct > Auto (FSM).
    // =========================================================================

    // 1. SubGear claim — SUBGEAR_BUS != 0 → force gear=1, claim.
    //    Rationale: Crawl mode active. Blocks manual/direct/auto modes — exclusive control.
    //    NOTE: kSubGearClaimBypass (forceValue=1) is mandatory — without it, the RPM value
    //    is passed unchanged as gear number (garbage) → rpm_to_speed overflow → saccade.
    { .name      = "subgear-claim",
      .inCh   = { AnalogComBusID::SUBGEAR_BUS },
      .fn        = cb_bypass_fn,
      .cfg       = &kSubGearClaimBypass,
    },

    // 2. Manual claim — MANUAL_GEAR_SET HIGH → passthrough GEAR, claim.
    //    Rationale: Manual mode active. GEAR already set by INPUT chain
    //    (cb_btn_inc/dec on UP/DOWN buttons). This proc prevents auto-FSM
    //    from overwriting the manual selection.
    //    NOTE: COMMENTED OUT — MANUAL_GEAR_SET not yet written by INPUT chain.
    //    Reading uninitialized digital channel causes random claims → saccades.
    //    TODO: Implement INPUT chain procs that write MANUAL_GEAR_SET before enabling.
    /*
    { .name      = "manual-claim",
      .inCh   = { DigitalComBusID::MANUAL_GEAR_SET },
      .fn        = cb_bypass_fn,  // claim if flag HIGH, value unchanged (GEAR set by INPUT)
    },
    */

    // 3. Direct Drive claim — DIRECT_DRIVE HIGH → force gear=1, claim.
    //    Rationale: Direct-drive bypass mode (inertia disabled on throttle).
    //    Fixed 1st gear for predictable direct ESC control.
    { .name      = "direct-claim",
      .inCh   = { DigitalComBusID::DIRECT_DRIVE },
      .fn        = cb_bypass_fn,
      .cfg       = &kDirectDriveGearBypass,  // force gear=1, claim
    },

    // 4. Auto FSM — RPM + direction → automatic gear (if no claim above).
    //    Rationale: Default mode. Hysteresis-based upshift/downshift driven
    //    by RPM thresholds and DRIVE_STATE (accel vs. decel detection).
    { .name      = "gear-fsm",
      .inCh   = { AnalogComBusID::DRIVE_STATE_BUS },
      .fn        = gear_fsm_fn,
      .cfg       = &kGearCfg,
      .state     = &gGearFsmState,
    },

    // 5. Gear→ramp bridge — updates gTractionRampDyn based on final gear.
    //    Rationale: Per-gear ramp time. Reads `value` (= gear after claim cascade),
    //    writes gTractionRampDyn.rampTimeMs. THROTTLE chain uses updated rampTime
    //    at NEXT cycle (1 cycle latency — acceptable, avoids circular dependency).
    { .name      = "gear-ramp",
      .fn        = gear_dyn_ramp_fn,
      .cfg       = &kGearCfg,
      .dynCfg    = &gTractionRampDyn,
    },
};

static CbProc kTractionProcs[] = {
    // rpm_to_speed — RPM + gear → ESC_SPEED_BUS bipolar.
    //   When GEAR = 0 (sub-gear sentinel), outputs CbusNeutral directly.
    { .name   = "rpm_to_speed",
      .inCh   = { AnalogComBusID::DRIVE_STATE_BUS, AnalogComBusID::GEAR },
      .fn     = gear_rpm_to_speed_fn,
      .cfg    = &kGearCfg,
    },
    // subgear-speed — overrides ESC_SPEED_BUS with sub-gear-capped speed.
    //   Passthrough (value unchanged) when SUBGEAR_BUS == 0 (normal mode).
    //   RPM_BUS is re-read via inCh[2] for the sub-gear speed calculation,
    //   independently of the CbusNeutral already in value from rpm_to_speed.
    { .name   = "subgear-speed",
      .inCh   = { AnalogComBusID::SUBGEAR_BUS,
                  AnalogComBusID::DRIVE_STATE_BUS,
                  AnalogComBusID::RPM_BUS },
      .fn     = gear_subgear_speed_fn,
      .cfg    = &kGearCfg,
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
    .outCh      = AnalogComBusID::RPM_BUS,
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
