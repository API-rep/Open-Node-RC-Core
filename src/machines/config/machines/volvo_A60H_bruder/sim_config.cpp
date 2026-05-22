/*!****************************************************************************
 * @file    sim_config.cpp
 * @brief   Volvo A60H Bruder — SimChannel pipeline configuration.
 *
 * @details Defines the SimChannel array for the Volvo A60H Bruder machine.
 *
 *   Channel pipelines:
 *     SIM_THROTTLE : THROTTLE_BUS  →  [bypass + ramp + center + abs + scale + ratio]  →  RPM_BUS (wire)
 *     SIM_GEAR     : RPM_BUS       →  [gear-fsm]                                       →  GEAR    (wire)
 *     SIM_TRACTION : RPM_BUS       →  [rpm_to_speed]                                   →  ESC_SPEED_BUS (wire, motors)
 *     SIM_STEERING : STEERING_BUS  →  [bypass + ramp]  →  STEERING_RAMPED_BUS (local)
 *     SIM_DUMP     : DUMP_BUS      →  [bypass + ramp]  →  DUMP_RAMPED_BUS     (local)
 *******************************************************************************
 */

#include "sim_config.h"

#include <core/config/machines/combus_types.h>    // AnalogComBusID (+ using namespace DumperTruck)
#include <core/config/hw/simulation_presets.h>    // kHeavy3_steps (for outMax ceiling)
#include <struct/combus_struct.h>                 // makeChanOwner, ComBusOwner
#include <core/system/combus/combus_res.h>        // CbusNeutral, pctToCbus
#include <core/system/simulation/sim_ramp.h>      // sim_ramp_fn, SimRampCfg, SimRampState
#include <core/system/simulation/sim_bypass.h>    // sim_bypass_fn, SimBypassCfg
#include <core/system/simulation/sim_math.h>       // sim_center_fn, sim_scale_fn, SimScaleCfg
#include <core/system/simulation/sim_gear.h>       // sim_gear_fn, sim_apply_ratio_fn, GearProcCfg, GearFsmState, ShiftDeltaState
#include <core/config/machines/dumper_truck/motion/dumper_truck_motion.h>  // kDumperTruckGearShift


// =============================================================================
// 1. RAMP CONFIGURATIONS (vehicle-specific asymmetric presets)
// =============================================================================

//  Steering — progressive start (5 %/tick @ 20 ms), instant stop (snap in 1 tick).
static constexpr SimRampCfg kSteerAsymRamp {
    .rampTimeMs  = 20u,
    .accelSteps  = pctToCbus(5),   ///< ~400 ms neutral-to-full
    .brakeSteps  = CbusNeutral,    ///< Instant return to neutral (1 tick max)
    .neutralBand = 0u,
};

//  Dump body — slow raise (2 %/tick), fast lower (4 %/tick), instant stop.
static constexpr SimRampCfg kDumpAsymRamp {
    .rampTimeMs     = 20u,
    .accelSteps     = pctToCbus(2),   ///< ~1 s neutral-to-full (montée)
    .accelDownSteps = pctToCbus(4),   ///< ~500 ms neutral-to-full (descente — moitié du temps)
    .brakeSteps     = CbusNeutral,    ///< Instant return to neutral (1 tick max)
    .neutralBand    = 0u,
};


//  Traction — heavy truck inertia (slow accel, moderate engine brake).
//  Steps are in RPM units (domain [0..maxRpm=2100]): 3 %/tick accel, 6 %/tick brake.
//  At 50 ms/tick: accel ~1.7 s, brake ~0.8 s  (same real-time as before the RPM-domain migration).
static constexpr SimRampCfg kTractionRamp {
    .rampTimeMs  = 50u,
    .accelSteps  = static_cast<combus_t>(3u * kHeavy3_steps[std::size(kHeavy3_steps) - 1u].upShift / 100u),
                            ///< 3 % of 2100 RPM = 63 RPM/step
    .brakeSteps  = static_cast<combus_t>(6u * kHeavy3_steps[std::size(kHeavy3_steps) - 1u].upShift / 100u),
                            ///< 6 % of 2100 RPM = 126 RPM/step
    .neutralBand = 0u,
};

//  Traction direct-drive bypass — OPTIONS button → DIRECT_DRIVE HIGH → ramp skipped.
static constexpr SimBypassCfg kTractionBypass {
    .condCh = DigitalComBusID::DIRECT_DRIVE,
};


//  Throttle → RPM mapping — center(signed) → abs → scale.
//  No direction side effect here: direction is already tracked by DRIVE_STATE_BUS (drive FSM).
static constexpr SimScaleCfg kThrottleScale {
    .inMax  = CbusNeutral,  ///< Magnitude after sim_center_fn is in [0..CbusNeutral].
    .outMax = static_cast<uint16_t>(kHeavy3_steps[std::size(kHeavy3_steps) - 1u].upShift),
                            ///< Full-stick RPM ceiling — top gear upShift from kHeavy3_steps.
};

//  Gear FSM + shift-delta proc shared config — both reference the same profile.
static constexpr GearProcCfg kGearCfg {
    .profile = kDumperTruckGearShift,
};


// =============================================================================
// 2. RUNTIME STATES (zero-init — self-snap to CbusNeutral on first call)
// =============================================================================

static SimRampState    gSteerRampState          {};
static SimRampState    gDumpRampState           {};
static SimRampState    gTractionRampState       {};
static GearFsmState    gGearFsmState            {};
static ShiftDeltaState gThrottleShiftDeltaState {};


// =============================================================================
// 3. PROCESSOR ARRAYS
// =============================================================================

static SimProc kThrottleProcs[] = {
    {
        .name    = "center",
        .optInCh = AnalogComBusID::THROTTLE_BUS,
        .fn      = sim_center_fn,
        .cfg     = nullptr,
        .state   = nullptr,
    },
    {
        .name    = "abs",
        .optInCh = AnalogComBusID::THROTTLE_BUS,  ///< Informational — value piped from center.
        .fn      = sim_abs_fn,
        .cfg     = nullptr,
        .state   = nullptr,
    },
    {
        .name    = "scale",
        .optInCh = AnalogComBusID::THROTTLE_BUS,  ///< Informational — value piped from abs.
        .fn      = sim_scale_fn,
        .cfg     = &kThrottleScale,
        .state   = nullptr,
    },
    {
        .name    = "bypass",
        .optInCh = AnalogComBusID::THROTTLE_BUS,  ///< DIRECT_DRIVE HIGH → skip ramp + ratio, scaled RPM → RPM_BUS direct.
        .fn      = sim_bypass_fn,
        .cfg     = &kTractionBypass,
        .state   = nullptr,
    },
    {
        .name    = "ramp",
        .optInCh = AnalogComBusID::THROTTLE_BUS,  ///< Inertia in RPM domain [0..maxRpm].
        .fn      = sim_ramp_fn,
        .cfg     = &kTractionRamp,
        .state   = &gTractionRampState,
    },
    {
        .name    = "ratio",
        .optInCh = AnalogComBusID::RPM_BUS,       ///< Reads GEAR from bus (previous tick) for upshift detection.
        .fn      = sim_apply_ratio_fn,
        .cfg     = &kGearCfg,
        .state   = &gThrottleShiftDeltaState,
    },
};

static SimProc kGearProcs[] = {
    {
        .name    = "bypass",
        .optInCh = AnalogComBusID::RPM_BUS,  ///< DIRECT_DRIVE HIGH → GEAR = 0 (no gear simulation).
        .fn      = sim_gear_bypass_fn,
        .cfg     = nullptr,
        .state   = nullptr,
    },
    {
        .name    = "gear-fsm",
        .optInCh = AnalogComBusID::RPM_BUS,  ///< Reads DRIVE_STATE_BUS for FWD gate; value seeded from RPM_BUS.
        .fn      = sim_gear_fn,
        .cfg     = &kGearCfg,
        .state   = &gGearFsmState,
    },
};

static SimProc kTractionProcs[] = {
    {
        .name    = "rpm_to_speed",
        .optInCh = AnalogComBusID::RPM_BUS,  ///< RPM post-inertia [0..maxRpm] → ESC_SPEED_BUS [0..CbusMaxVal] bipolar.
        .fn      = sim_rpm_to_speed_fn,
        .cfg     = &kGearCfg,
        .state   = nullptr,
    },
};

static SimProc kSteeringProcs[] = {
    {
        .name    = "bypass",
        .optInCh = AnalogComBusID::STEERING_BUS,
        .fn      = sim_bypass_fn,
        .cfg     = &kTractionBypass,
        .state   = nullptr,
    },
    {
        .name    = "ramp",
        .optInCh = AnalogComBusID::STEERING_BUS,  ///< Informational — value seeded by SimChannel
        .fn      = sim_ramp_fn,
        .cfg     = &kSteerAsymRamp,
        .state   = &gSteerRampState,
    },
};

static SimProc kDumpProcs[] = {
    {
        .name    = "bypass",
        .optInCh = AnalogComBusID::DUMP_BUS,
        .fn      = sim_bypass_fn,
        .cfg     = &kTractionBypass,
        .state   = nullptr,
    },
    {
        .name    = "ramp",
        .optInCh = AnalogComBusID::DUMP_BUS,      ///< Informational — value seeded by SimChannel
        .fn      = sim_ramp_fn,
        .cfg     = &kDumpAsymRamp,
        .state   = &gDumpRampState,
    },
};


// =============================================================================
// 4. CHANNEL ARRAY
// =============================================================================

SimChannel kSimChannels[SIM_CH_COUNT] = {

  {
    .name         = "throttle",
    .inCh         = AnalogComBusID::THROTTLE_BUS,
    .outCh        = AnalogComBusID::RPM_BUS,       ///< Engine RPM magnitude [0..maxRpm] — post-inertia, post-ratio. Transmitted to sound node.
    .simProc      = kThrottleProcs,
    .simProcCount = 6u,                            ///< bypass(0) + ramp(1) + center(2) + abs(3) + scale(4) + ratio(5)
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },

  {
    .name         = "gear",
    .inCh         = AnalogComBusID::RPM_BUS,
    .outCh        = AnalogComBusID::GEAR,          ///< Active gear index (1..N) — read by SIM_TRACTION ramp + sound node.
    .simProc      = kGearProcs,
    .simProcCount = 2u,                            ///< bypass(0) + gear-fsm(1)
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },

  {
    .name         = "traction",
    .inCh         = AnalogComBusID::RPM_BUS,
    .outCh        = AnalogComBusID::ESC_SPEED_BUS, ///< RPM-derived speed → motors (wire). Gear-accumulation formula, direction from DRIVE_STATE_BUS.
    .simProc      = kTractionProcs,
    .simProcCount = 1u,                            ///< rpm_to_speed(0)
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },

  {
    .name         = "steering",
    .inCh         = AnalogComBusID::STEERING_BUS,
    .outCh        = AnalogComBusID::STEERING_RAMPED_BUS,
    .simProc      = kSteeringProcs,
    .simProcCount = 2u,                         ///< bypass(0) + ramp(1)
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },

  {
    .name         = "dump",
    .inCh         = AnalogComBusID::DUMP_BUS,
    .outCh        = AnalogComBusID::DUMP_RAMPED_BUS,
    .simProc      = kDumpProcs,
    .simProcCount = 2u,                         ///< bypass(0) + ramp(1)
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },
};


// EOF sim_config.cpp
