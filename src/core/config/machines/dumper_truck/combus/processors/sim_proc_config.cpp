/*!****************************************************************************
 * @file  sim_proc_config.cpp
 * @brief Dumper truck — CbChain pipeline configuration (SIM layer).
 *
 * @details Defines the SIM CbChain array for the dumper-truck machine class.
 *   Compiled only in machine-node builds (IS_MAINBOARD guard).
 *
 *   Channel pipelines (in → procs → out):
 *     SIM_THROTTLE : in(THROTTLE_BUS) → ramp, dir(→DRIVE_STATE_BUS),
 *                    b-in(BRAKE_BUS→state), brake(THROTTLE_BUS→BRAKE_BUS merged+ramp),
 *                    center, abs, scale, bypass(DIRECT_DRIVE),
 *                    ratio(GEAR) → out(RPM_BUS)
 *     SIM_GEAR     : in(RPM_BUS) → subgear-claim(SUBGEAR_BUS),
 *                    manual-claim(MANUAL_GEAR_SET), direct-claim(DIRECT_DRIVE),
 *                    gear-fsm(DRIVE_STATE_BUS), gear-ramp → out(GEAR)
 *                    NOTE: subgear-claim forces GEAR=1 when sub-gear active.
 *     SIM_TRACTION : in(RPM_BUS) → gear-ratio(GEAR), subgear-cap(SUBGEAR_BUS),
 *                    gear-dir(DRIVE_STATE_BUS) → out(ESC_SPEED_BUS)
 *                    NOTE: gear-ratio passes through when GEAR=0 (neutral);
 *                    gear-subgear-cap caps magnitude in RPM domain;
 *                    gear-dir applies direction once at end.
 *     SIM_STEERING : in(STEERING_BUS) → bypass(DIRECT_DRIVE), ramp → out(STEERING_RAMPED_BUS)
 *     SIM_DUMP     : in(DUMP_BUS) → bypass(DIRECT_DRIVE), ramp → out(DUMP_RAMPED_BUS)
 *
 *   SUBGEAR_BUS is written by INPUT chain (cb_btn procs) — see input_proc_config.cpp.
 *******************************************************************************
 */

#ifdef IS_MAINBOARD

#include "sim_proc_config.h"

#include <core/config/machines/dumper_truck/combus/combus.h>  // AnalogComBusID, DigitalComBusID
#include <core/config/hw/simulation_presets.h>    // kHeavy6_steps, kGearShift_Heavy6Speed
#include <struct/combus_struct.h>                 // makeChanOwner, ComBusOwner
#include <core/system/combus/combus_res.h>        // CbusNeutral, pctToCbus
#include <core/system/combus/processors/motion/cb_ramp.h>  // cb_ramp_fn, CbRampCfg, CbRampState
#include <core/system/combus/processors/base/cb_bypass.h>  // cb_bypass_fn
#include <core/system/combus/processors/base/cb_io.h>       // cb_in_fn, cb_out_fn
#include <core/system/combus/processors/math/cb_center.h>          // cb_center_fn, CbCenterCfg
#include <core/system/combus/processors/math/cb_abs.h>              // cb_abs_fn
#include <core/system/combus/processors/math/cb_scale.h>            // cb_scale_fn, CbScaleCfg
#include <core/system/combus/processors/motion/cb_dir.h>           // cb_dir_fn, CbDirCfg
#include <core/system/combus/processors/motion/cb_brake.h>         // cb_brake_fn, cb_rev_brake_fn
#include <core/system/combus/processors/modules/gear/cb_gear.h>    // gear_fsm_fn, gear_upshift_drop_fn, gear_ratio_fn, gear_subgear_cap_fn, gear_dir_fn, gear_dyn_ramp_fn
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
    .brakeSteps  = pctToCbus(1),   ///< Near-zero natural decel — vehicle coasts freely; active braking via cb_rev_brake_fn.
    .neutralBand = pctToCbus(8),   ///< 8 % deadzone — absorbs PS4 stick noise (±~0.4%) at neutral,
                                   ///<   preventing cb_dir_fn from oscillating FWD↔REV at rest.
                                   ///<   Also used as revBrakeDeadBand in kBrakeCfg.
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
    .outMax = static_cast<uint16_t>(kHeavy6_steps[std::size(kHeavy6_steps) - 1u].upShift),
    .outMin = static_cast<uint16_t>(kHeavy6_steps[0].downShift),  ///< Idle RPM floor (700) — gear[0].downShift repurposed storage.
};

static constexpr GearProcCfg kGearCfg {
    .profile = &kGearShift_Heavy6Speed,
};

static constexpr CbBrakeCfg kBrakeCfg {
    .maxBrake          = CbusNeutral,       ///< Full opposing stick → full stop.
    .reverseHoldMs     = 400u,              ///< 400 ms hold at standstill before direction reversal allowed.
    .dynRampCfg        = &gTractionRampDyn,
    .maxBrakeStep      = pctToCbus(25),     ///< extBrakeSteps ceiling at full brake — tune on hardware.
    .brakeScalePct     = 50u,              ///< Brake-input max deflection = 50 % of maxBrake.
    .revBrakeScalePct  = 150u,             ///< Reverse-stick 1.5× amplification — tune on hardware.
    .revBrakeDeadBand  = kTractionRamp.neutralBand, ///< Must match ramp neutralBand — stick within the ramp dead zone
                                           ///<   must not arm revBraking.
};

static constexpr CbBypassCfg kSubGearClaimBypass {
    .forceValue = 1u,  ///< Force GEAR = 1 when sub-gear mode active.
                       ///< gear_ratio_fn passes through (GEAR=1 → neutral cumDelta);
                       ///< gear_subgear_cap_fn handles the actual speed cap.
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
static CbBrakeState       gBrakeState              {};
static GearFsmState        gGearFsmState            {};
static ShiftDeltaState     gThrottleShiftDeltaState {};


// =============================================================================
// 4. PROCESSOR ARRAYS
// =============================================================================

static CbProc kThrottleProcs[] = {
    // in — seed pipeline from THROTTLE_BUS.
    { .name  = "in",
      .inCh  = AnalogComBusID::THROTTLE_BUS,
      .fn    = cb_in_fn,
    },
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
    // b-in — cache normalized brake input from BRAKE_BUS into gBrakeState.brakeVal each cycle.
    //   Observer; does not modify value.
    { .name  = "b-in",
      .inCh  = AnalogComBusID::BRAKE_BUS,
      .fn    = cb_brake_fn,
      .state = &gBrakeState,
    },
    // brake — merge reverse-brake (opposing stick vs ramp) with brake input, write BRAKE_BUS.
    //   Also dynamically scales ramp brakeSteps (naturalBrakeStep → maxBrakeStep at full force).
    //   maxBrake = CbusNeutral; brakeScalePct = 50 (brake-input max = 50 % of maxBrake).
    //   reverseHoldMs = 400 ms hold at standstill before direction reversal allowed.
    //   Observer: does not modify value; side-effect writes to BRAKE_BUS and dynRampCfg.
    { .name  = "brake",
      .inCh  = AnalogComBusID::THROTTLE_BUS,
      .outCh = AnalogComBusID::BRAKE_BUS,
      .fn    = cb_rev_brake_fn,
      .cfg   = &kBrakeCfg,
      .state = &gBrakeState,
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
      .inCh      = DigitalComBusID::DIRECT_DRIVE,
      .fn        = cb_bypass_fn,
    },
    // ratio — subtract shiftDelta RPM on upshift.
    { .name      = "ratio",
      .inCh      = AnalogComBusID::GEAR,
      .fn        = gear_upshift_drop_fn,
      .cfg       = &kGearCfg,
      .state     = &gThrottleShiftDeltaState,
    },
    // out — commit pipeline value to RPM_BUS.
    { .name  = "out",
      .outCh = AnalogComBusID::RPM_BUS,
      .fn    = cb_out_fn,
    },
};

static CbProc kGearProcs[] = {
    // in — seed pipeline from RPM_BUS.
    { .name  = "in",
      .inCh  = AnalogComBusID::RPM_BUS,
      .fn    = cb_in_fn,
    },
    // =========================================================================
    // CLAIM CASCADE — first claim wins, later procs skipped.
    // Priority: SubGear > Manual > Direct > Auto (FSM).
    // =========================================================================

    // 1. SubGear claim — SUBGEAR_BUS != 0 → force gear=1, claim.
    //    Rationale: Crawl mode active. Blocks manual/direct/auto modes — exclusive control.
    { .name      = "subgear-claim",
      .inCh      = AnalogComBusID::SUBGEAR_BUS,
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
      .inCh      = { DigitalComBusID::MANUAL_GEAR_SET },
      .fn        = cb_bypass_fn,  // claim if flag HIGH, value unchanged (GEAR set by INPUT)
    },
    */

    // 3. Direct Drive claim — DIRECT_DRIVE HIGH → force gear=1, claim.
    //    Rationale: Direct-drive bypass mode (inertia disabled on throttle).
    //    Fixed 1st gear for predictable direct ESC control.
    { .name      = "direct-claim",
      .inCh      = DigitalComBusID::DIRECT_DRIVE,
      .fn        = cb_bypass_fn,
      .cfg       = &kDirectDriveGearBypass,  // force gear=1, claim
    },

    // 4. Auto FSM — RPM + direction → automatic gear (if no claim above).
    //    Rationale: Default mode. Hysteresis-based upshift/downshift driven
    //    by RPM thresholds and DRIVE_STATE (accel vs. decel detection).
    { .name      = "gear-fsm",
      .inCh      = AnalogComBusID::DRIVE_STATE_BUS,
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
    // out — commit pipeline value to GEAR.
    { .name  = "out",
      .outCh = AnalogComBusID::GEAR,
      .fn    = cb_out_fn,
    },
};

static CbProc kTractionProcs[] = {
    // in — seed pipeline from RPM_BUS.
    { .name  = "in",
      .inCh  = AnalogComBusID::RPM_BUS,
      .fn    = cb_in_fn,
    },
    // gear-ratio — RPM + cumDelta[gear] → adjustedRpm; GEAR=0 → passthrough.
    { .name  = "gear-ratio",
      .inCh  = AnalogComBusID::GEAR,
      .fn    = gear_ratio_fn,
      .cfg   = &kGearCfg,
    },
    // subgear-cap — cap magnitude to maxSpeedPct when SUBGEAR active; passthrough if 0.
    { .name  = "subgear-cap",
      .inCh  = AnalogComBusID::SUBGEAR_BUS,
      .fn    = gear_subgear_cap_fn,
      .cfg   = &kGearCfg,
    },
    // gear-dir — apply direction (DRIVE_STATE_BUS) → bipolar ESC_SPEED_BUS.
    { .name  = "gear-dir",
      .inCh  = AnalogComBusID::DRIVE_STATE_BUS,
      .fn    = gear_dir_fn,
      .cfg   = &kGearCfg,
    },
    // out — commit pipeline value to ESC_SPEED_BUS.
    { .name  = "out",
      .outCh = AnalogComBusID::ESC_SPEED_BUS,
      .fn    = cb_out_fn,
    },
};

static CbProc kSteeringProcs[] = {
    // in — seed pipeline from STEERING_BUS.
    { .name  = "in",
      .inCh  = AnalogComBusID::STEERING_BUS,
      .fn    = cb_in_fn,
    },
    // bypass — DIRECT_DRIVE HIGH → claim (raw steering bypasses ramp).
    { .name    = "bypass",
      .inCh    = DigitalComBusID::DIRECT_DRIVE,
      .fn      = cb_bypass_fn,
    },
    // ramp — progressive inertia.
    { .name  = "ramp",
      .fn    = cb_ramp_fn,
      .cfg   = &kSteerAsymRamp,
      .state = &gSteerRampState,
    },
    // out — commit pipeline value to STEERING_RAMPED_BUS.
    { .name  = "out",
      .outCh = AnalogComBusID::STEERING_RAMPED_BUS,
      .fn    = cb_out_fn,
    },
};

static CbProc kDumpProcs[] = {
    // in — seed pipeline from DUMP_BUS.
    { .name  = "in",
      .inCh  = AnalogComBusID::DUMP_BUS,
      .fn    = cb_in_fn,
    },
    // bypass — DIRECT_DRIVE HIGH → claim (raw dump bypasses ramp).
    { .name    = "bypass",
      .inCh    = DigitalComBusID::DIRECT_DRIVE,
      .fn      = cb_bypass_fn,
    },
    // ramp — progressive asymmetric inertia.
    { .name  = "ramp",
      .fn    = cb_ramp_fn,
      .cfg   = &kDumpAsymRamp,
      .state = &gDumpRampState,
    },
    // out — commit pipeline value to DUMP_RAMPED_BUS.
    { .name  = "out",
      .outCh = AnalogComBusID::DUMP_RAMPED_BUS,
      .fn    = cb_out_fn,
    },
};


// =============================================================================
// 5. CHANNEL ARRAY
// =============================================================================

static constexpr ChanOwner kSimOwner = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM);

CbChain kSimChannels[SIM_CH_COUNT] = {

  { .name       = "throttle",
    .procs      = kThrottleProcs,
    .procCount  = static_cast<uint8_t>(std::size(kThrottleProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "gear",
    .procs      = kGearProcs,
    .procCount  = static_cast<uint8_t>(std::size(kGearProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "traction",
    .procs      = kTractionProcs,
    .procCount  = static_cast<uint8_t>(std::size(kTractionProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "steering",
    .procs      = kSteeringProcs,
    .procCount  = static_cast<uint8_t>(std::size(kSteeringProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "dump",
    .procs      = kDumpProcs,
    .procCount  = static_cast<uint8_t>(std::size(kDumpProcs)),
    .chainOwner = kSimOwner,
  },
};

#endif  // IS_MAINBOARD

// EOF sim_proc_config.cpp
