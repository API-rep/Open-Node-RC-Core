/*!****************************************************************************
 * @file    sim_config.cpp
 * @brief   Volvo A60H Bruder — SimChannel pipeline configuration.
 *
 * @details Defines the SimChannel array for the Volvo A60H Bruder machine.
 *
 *   Channel pipelines (each proc array is fully self-describing: read → … → write):
 *     SIM_THROTTLE : read(THROTTLE_BUS) → ramp → drive-state(→DRIVE_STATE_BUS) → center → abs → scale → bypass(DIRECT_DRIVE→RPM_BUS) → ratio → write(RPM_BUS)
 *     SIM_GEAR     : read(RPM_BUS)      → bypass(DIRECT_DRIVE→GEAR,=1) → gear-fsm → write(GEAR)
 *     SIM_TRACTION : read(RPM_BUS)      → rpm_to_speed → write(ESC_SPEED_BUS)
 *     SIM_STEERING : read(STEERING_BUS) → bypass(DIRECT_DRIVE→STEERING_RAMPED_BUS) → ramp → write(STEERING_RAMPED_BUS)
 *     SIM_DUMP     : read(DUMP_BUS)     → bypass(DIRECT_DRIVE→DUMP_RAMPED_BUS) → ramp → write(DUMP_RAMPED_BUS)
 *******************************************************************************
 */

#include "sim_config.h"

#include <core/config/machines/combus_types.h>    // AnalogComBusID (+ using namespace DumperTruck)
#include <core/config/hw/simulation_presets.h>    // kHeavy3_steps (for outMax ceiling)
#include <struct/combus_struct.h>                 // makeChanOwner, ComBusOwner
#include <core/system/combus/combus_res.h>        // CbusNeutral, pctToCbus
#include <core/system/simulation/sim_io.h>        // sim_read_fn, sim_write_fn
#include <core/system/simulation/sim_ramp.h>      // sim_ramp_fn, SimRampCfg, SimRampState
#include <core/system/simulation/sim_bypass.h>    // sim_bypass_fn, SimBypassCfg
#include <core/system/simulation/sim_math.h>      // sim_center_fn, sim_abs_fn, sim_scale_fn, sim_drive_state_fn, SimCenterCfg, SimDriveStateCfg, SimScaleCfg
#include <core/system/simulation/sim_gear.h>      // sim_gear_fn, sim_apply_ratio_fn, GearProcCfg, GearFsmState, ShiftDeltaState
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
//  Steps in bipolaire ComBus domain [0..CbusMaxVal]: 3 %/tick accel, 6 %/tick brake.
//  At 50 ms/tick: accel ~1.7 s neutral-to-full, brake ~0.8 s.
static constexpr SimRampCfg kTractionRamp {
    .rampTimeMs  = 50u,
    .accelSteps  = pctToCbus(3),   ///< 3 % of CbusNeutral per tick (bipolaire domain)
    .brakeSteps  = pctToCbus(6),   ///< 6 % of CbusNeutral per tick
    .neutralBand = 0u,
};

//  Traction ramp — RAM copy, mutated by sim_gear_fn on each gear change.
//  dynCfg on the traction ramp proc points here; GearFsmState::rampDynCfg also points here.
static SimRampCfg gTractionRampDyn = kTractionRamp;


// =============================================================================
// 2. BYPASS CONFIGURATIONS (per-channel — condCh + outCh)
// =============================================================================

//  Throttle bypass: DIRECT_DRIVE HIGH → skip ramp + ratio, scaled RPM → RPM_BUS direct.
static constexpr SimBypassCfg kThrottleBypass {
    .condCh = DigitalComBusID::DIRECT_DRIVE,
    .outCh  = AnalogComBusID::RPM_BUS,
};

//  Gear bypass: DIRECT_DRIVE HIGH → GEAR = 1 (locked, no upshifting).
//  Uses sim_gear_bypass_fn which writes 1 (not the piped RPM value).
static constexpr SimBypassCfg kGearBypass {
    .condCh = DigitalComBusID::DIRECT_DRIVE,
    .outCh  = AnalogComBusID::GEAR,
};

//  Steering bypass: DIRECT_DRIVE HIGH → STEERING_BUS → STEERING_RAMPED_BUS direct.
static constexpr SimBypassCfg kSteeringBypass {
    .condCh = DigitalComBusID::DIRECT_DRIVE,
    .outCh  = AnalogComBusID::STEERING_RAMPED_BUS,
};

//  Dump bypass: DIRECT_DRIVE HIGH → DUMP_BUS → DUMP_RAMPED_BUS direct.
static constexpr SimBypassCfg kDumpBypass {
    .condCh = DigitalComBusID::DIRECT_DRIVE,
    .outCh  = AnalogComBusID::DUMP_RAMPED_BUS,
};


// =============================================================================
// 3. SCALE + GEAR CONFIG
// =============================================================================

//  Throttle center — standard ComBus bipolar neutral (CbusNeutral = 32767).
static constexpr SimCenterCfg kThrottleCenter {};

//  Throttle drive-state — standard ComBus bipolar neutral.
static constexpr SimDriveStateCfg kThrottleDriveState {};

//  Throttle → RPM mapping — center(signed) → abs → scale.
//  No direction side effect here: direction is already tracked by DRIVE_STATE_BUS.
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
// 4. RUNTIME STATES (zero-init — self-snap to CbusNeutral on first call)
// =============================================================================

static SimRampState    gSteerRampState          {};
static SimRampState    gDumpRampState           {};
static SimRampState    gTractionRampState       {};
static GearFsmState    gGearFsmState            { .rampDynCfg = &gTractionRampDyn };
static ShiftDeltaState gThrottleShiftDeltaState {};


// =============================================================================
// 5. PROCESSOR ARRAYS
// =============================================================================

static SimProc kThrottleProcs[] = {
    {
        .name   = "read",
        .optInCh  = AnalogComBusID::THROTTLE_BUS,
        .fn     = sim_read_fn,
        .cfg    = nullptr,
        .state  = nullptr,
    },
    {
        .name  = "ramp",
        .fn    = sim_ramp_fn,
        .cfg   = &kTractionRamp,
        .dynCfg = &gTractionRampDyn,
        .state = &gTractionRampState,
    },
    {
        .name    = "drive-state",
        .optOutCh  = AnalogComBusID::DRIVE_STATE_BUS,
        .fn      = sim_drive_state_fn,
        .cfg     = &kThrottleDriveState,
        .state   = nullptr,
    },
    {
        .name  = "center",
        .fn    = sim_center_fn,
        .cfg   = &kThrottleCenter,
        .state = nullptr,
    },
    {
        .name  = "abs",
        .fn    = sim_abs_fn,
        .cfg   = nullptr,
        .state = nullptr,
    },
    {
        .name  = "scale",
        .fn    = sim_scale_fn,
        .cfg   = &kThrottleScale,
        .state = nullptr,
    },
    {
        .name  = "bypass",
        .fn    = sim_bypass_fn,
        .cfg   = &kThrottleBypass,
        .state = nullptr,
    },
    {
        .name  = "ratio",
        .fn    = sim_apply_ratio_fn,
        .cfg   = &kGearCfg,
        .state = &gThrottleShiftDeltaState,
    },
    {
        .name    = "write",
        .optOutCh  = AnalogComBusID::RPM_BUS,
        .fn      = sim_write_fn,
        .cfg     = nullptr,
        .state   = nullptr,
    },
};

static SimProc kGearProcs[] = {
    {
        .name   = "read",
        .optInCh  = AnalogComBusID::RPM_BUS,
        .fn     = sim_read_fn,
        .cfg    = nullptr,
        .state  = nullptr,
    },
    {
        .name    = "bypass",
        .fn      = sim_gear_bypass_fn,
        .cfg     = &kGearBypass,
        .state   = nullptr,
    },
    {
        .name    = "gear-fsm",
        .fn      = sim_gear_fn,
        .cfg     = &kGearCfg,
        .state   = &gGearFsmState,
    },
    {
        .name    = "write",
        .optOutCh  = AnalogComBusID::GEAR,
        .fn      = sim_write_fn,
        .cfg     = nullptr,
        .state   = nullptr,
    },
};

static SimProc kTractionProcs[] = {
    {
        .name   = "read",
        .optInCh  = AnalogComBusID::RPM_BUS,
        .fn     = sim_read_fn,
        .cfg    = nullptr,
        .state  = nullptr,
    },
    {
        .name    = "rpm_to_speed",
        .fn      = sim_rpm_to_speed_fn,
        .cfg     = &kGearCfg,
        .state   = nullptr,
    },
    {
        .name    = "write",
        .optOutCh  = AnalogComBusID::ESC_SPEED_BUS,
        .fn      = sim_write_fn,
        .cfg     = nullptr,
        .state   = nullptr,
    },
};

static SimProc kSteeringProcs[] = {
    {
        .name   = "read",
        .optInCh  = AnalogComBusID::STEERING_BUS,
        .fn     = sim_read_fn,
        .cfg    = nullptr,
        .state  = nullptr,
    },
    {
        .name    = "bypass",
        .fn      = sim_bypass_fn,
        .cfg     = &kSteeringBypass,
        .state   = nullptr,
    },
    {
        .name    = "ramp",
        .fn      = sim_ramp_fn,
        .cfg     = &kSteerAsymRamp,
        .state   = &gSteerRampState,
    },
    {
        .name    = "write",
        .optOutCh  = AnalogComBusID::STEERING_RAMPED_BUS,
        .fn      = sim_write_fn,
        .cfg     = nullptr,
        .state   = nullptr,
    },
};

static SimProc kDumpProcs[] = {
    {
        .name   = "read",
        .optInCh  = AnalogComBusID::DUMP_BUS,
        .fn     = sim_read_fn,
        .cfg    = nullptr,
        .state  = nullptr,
    },
    {
        .name    = "bypass",
        .fn      = sim_bypass_fn,
        .cfg     = &kDumpBypass,
        .state   = nullptr,
    },
    {
        .name    = "ramp",
        .fn      = sim_ramp_fn,
        .cfg     = &kDumpAsymRamp,
        .state   = &gDumpRampState,
    },
    {
        .name    = "write",
        .optOutCh  = AnalogComBusID::DUMP_RAMPED_BUS,
        .fn      = sim_write_fn,
        .cfg     = nullptr,
        .state   = nullptr,
    },
};


// =============================================================================
// 6. CHANNEL ARRAY
// =============================================================================

SimChannel kSimChannels[SIM_CH_COUNT] = {

  {
    .name         = "throttle",
    .simProc      = kThrottleProcs,
    .simProcCount = static_cast<uint8_t>(std::size(kThrottleProcs)),
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },

  {
    .name         = "gear",
    .simProc      = kGearProcs,
    .simProcCount = static_cast<uint8_t>(std::size(kGearProcs)),
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },

  {
    .name         = "traction",
    .simProc      = kTractionProcs,
    .simProcCount = static_cast<uint8_t>(std::size(kTractionProcs)),
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },

  {
    .name         = "steering",
    .simProc      = kSteeringProcs,
    .simProcCount = static_cast<uint8_t>(std::size(kSteeringProcs)),
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },

  {
    .name         = "dump",
    .simProc      = kDumpProcs,
    .simProcCount = static_cast<uint8_t>(std::size(kDumpProcs)),
    .chanOwner    = makeChanOwner(ComBusOwner::GRP_MACHINE, ComBusOwner::PROC_SYSTEM)
  },
};


// EOF sim_config.cpp
