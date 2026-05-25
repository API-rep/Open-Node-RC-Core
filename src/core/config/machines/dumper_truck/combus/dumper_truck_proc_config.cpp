/*!****************************************************************************
 * @file  dumper_truck_proc_config.cpp
 * @brief Dumper truck — CbChain pipeline configuration.
 *
 * @details Defines the CbChain array for the dumper-truck machine class.
 *   Compiled only in machine-node builds (IS_MAINBOARD guard).
 *
 *   Channel pipelines (optInCh → procs → optOutCh):
 *     SIM_THROTTLE : THROTTLE_BUS → ramp, drive-state(→DRIVE_STATE_BUS),
 *                    center, abs, scale, bypass(DIRECT_DRIVE),
 *                    ratio(DIRECT_DRIVE, GEAR) → RPM_BUS
 *     SIM_GEAR     : RPM_BUS → bypass(DIRECT_DRIVE), subgear-btn(→SUBGEAR_BUS),
 *                    gear-fsm(DRIVE_STATE_BUS, SUBGEAR_BUS),
 *                    gear-ramp(SUBGEAR_BUS) → GEAR
 *     SIM_TRACTION : RPM_BUS → rpm_to_speed(DRIVE_STATE_BUS, GEAR) → ESC_SPEED_BUS
 *     SIM_STEERING : STEERING_BUS → bypass(DIRECT_DRIVE), ramp → STEERING_RAMPED_BUS
 *     SIM_DUMP     : DUMP_BUS     → bypass(DIRECT_DRIVE), ramp → DUMP_RAMPED_BUS
 *******************************************************************************
 */

#ifdef IS_MAINBOARD

#include "dumper_truck_proc_config.h"

#include <core/config/machines/combus_types.h>    // AnalogComBusID (+ using namespace DumperTruck)
#include <core/config/hw/simulation_presets.h>    // kHeavy3_steps
#include <struct/combus_struct.h>                 // makeChanOwner, ComBusOwner
#include <core/system/combus/combus_res.h>        // CbusNeutral, pctToCbus
#include <core/system/simulation/sim_ramp.h>      // sim_ramp_fn, SimRampCfg, SimRampState
#include <core/system/simulation/sim_bypass.h>    // sim_bypass_fn
#include <core/system/simulation/sim_math.h>      // sim_center_fn, sim_abs_fn, sim_scale_fn, sim_drive_state_fn
#include <core/system/simulation/sim_gear.h>      // sim_gear_fn, sim_apply_ratio_fn, sim_gear_bypass_fn, sim_rpm_to_speed_fn, sim_gear_ramp_fn
#include <core/system/simulation/sim_subgear_btn.h>  // sim_subgear_btn_fn
#include <core/config/machines/dumper_truck/motion/dumper_truck_motion.h>  // kDumperTruckGearShift


// =============================================================================
// 1. RAMP CONFIGURATIONS
// =============================================================================

//  Steering — progressive start (5 %/tick @ 20 ms), instant stop.
static constexpr SimRampCfg kSteerAsymRamp {
    .rampTimeMs  = 20u,
    .accelSteps  = pctToCbus(5),
    .brakeSteps  = CbusNeutral,
    .neutralBand = 0u,
};

//  Dump body — slow raise, fast lower, instant stop.
static constexpr SimRampCfg kDumpAsymRamp {
    .rampTimeMs     = 20u,
    .accelSteps     = pctToCbus(2),
    .accelDownSteps = pctToCbus(4),
    .brakeSteps     = CbusNeutral,
    .neutralBand    = 0u,
};

//  Traction — heavy truck inertia.
static constexpr SimRampCfg kTractionRamp {
    .rampTimeMs  = 50u,
    .accelSteps  = pctToCbus(3),
    .brakeSteps  = pctToCbus(6),
    .neutralBand = 0u,
};

//  Traction ramp — RAM copy, mutated by sim_gear_ramp_fn on each gear change.
static SimRampCfg gTractionRampDyn = kTractionRamp;


// =============================================================================
// 2. MATH + GEAR CONFIGS
// =============================================================================

static constexpr SimCenterCfg kThrottleCenter {};
static constexpr SimDriveStateCfg kThrottleDriveState {};

static constexpr SimScaleCfg kThrottleScale {
    .inMax  = CbusNeutral,
    .outMax = static_cast<uint16_t>(kHeavy3_steps[std::size(kHeavy3_steps) - 1u].upShift),
};

static constexpr GearProcCfg kGearCfg {
    .profile = kDumperTruckGearShift,
};

static constexpr SimSubGearBtnCfg kSubGearBtnCfg {
    .subGearCount = static_cast<uint8_t>(kDumperTruckGearShift->subGearCount),
    .debounceMs   = 50u,
};


// =============================================================================
// 3. RUNTIME STATES
// =============================================================================

static SimRampState        gSteerRampState          {};
static SimRampState        gDumpRampState           {};
static SimRampState        gTractionRampState       {};
static GearFsmState        gGearFsmState            {};
static ShiftDeltaState     gThrottleShiftDeltaState {};
static SimSubGearBtnState  gSubGearBtnState         {};


// =============================================================================
// 4. PROCESSOR ARRAYS
// =============================================================================

static CbProc kThrottleProcs[] = {
    // ramp — heavy-truck inertia on throttle input.
    { .name    = "ramp",
      .fn      = sim_ramp_fn,
      .cfg     = &kTractionRamp,
      .dynCfg  = &gTractionRampDyn,
      .state   = &gTractionRampState,
    },
    // drive-state — side-effect: encodes direction → DRIVE_STATE_BUS.
    { .name         = "drive-state",
      .optSecOutCh  = AnalogComBusID::DRIVE_STATE_BUS,
      .fn           = sim_drive_state_fn,
      .cfg          = &kThrottleDriveState,
      .state        = nullptr,
    },
    // center — signed deviation from CbusNeutral.
    { .name  = "center",
      .fn    = sim_center_fn,
      .cfg   = &kThrottleCenter,
      .state = nullptr,
    },
    // abs — magnitude; sign side effect is unused here (direction tracked above).
    { .name  = "abs",
      .fn    = sim_abs_fn,
      .cfg   = nullptr,
      .state = nullptr,
    },
    // scale — RPM magnitude in [0..maxRpm].
    { .name  = "scale",
      .fn    = sim_scale_fn,
      .cfg   = &kThrottleScale,
      .state = nullptr,
    },
    // bypass — DIRECT_DRIVE HIGH → claim (raw magnitude bypasses ratio, flows to RPM_BUS).
    { .name      = "bypass",
      .secInCh   = { DigitalComBusID::DIRECT_DRIVE },
      .fn        = sim_bypass_fn,
      .cfg       = nullptr,
      .state     = nullptr,
    },
    // ratio — subtract shiftDelta on upshift (when not direct-drive).
    { .name      = "ratio",
      .secInCh   = { DigitalComBusID::DIRECT_DRIVE, AnalogComBusID::GEAR },
      .fn        = sim_apply_ratio_fn,
      .cfg       = &kGearCfg,
      .state     = &gThrottleShiftDeltaState,
    },
};

static CbProc kGearProcs[] = {
    // bypass — DIRECT_DRIVE HIGH → set gear=1, claim (skips gear-fsm).
    { .name      = "bypass",
      .secInCh   = { DigitalComBusID::DIRECT_DRIVE },
      .fn        = sim_gear_bypass_fn,
      .cfg       = nullptr,
      .state     = nullptr,
    },
    // subgear-btn — reads 3 buttons; writes SUBGEAR_BUS via secOutValue.
    { .name         = "subgear-btn",
      .secInCh      = { DigitalComBusID::SUBGEAR_SET_BTN,
                        DigitalComBusID::SUBGEAR_UP_BTN,
                        DigitalComBusID::SUBGEAR_DOWN_BTN },
      .optSecOutCh  = AnalogComBusID::SUBGEAR_BUS,
      .fn           = sim_subgear_btn_fn,
      .cfg          = &kSubGearBtnCfg,
      .state        = &gSubGearBtnState,
    },
    // gear-fsm — RPM → gear FSM; reads DRIVE_STATE_BUS + SUBGEAR_BUS.
    { .name      = "gear-fsm",
      .secInCh   = { AnalogComBusID::DRIVE_STATE_BUS, AnalogComBusID::SUBGEAR_BUS },
      .fn        = sim_gear_fn,
      .cfg       = &kGearCfg,
      .state     = &gGearFsmState,
    },
    // gear-ramp — updates traction ramp dynCfg for new gear; reads SUBGEAR_BUS.
    { .name      = "gear-ramp",
      .secInCh   = { AnalogComBusID::SUBGEAR_BUS },
      .fn        = sim_gear_ramp_fn,
      .cfg       = &kGearCfg,
      .dynCfg    = &gTractionRampDyn,
      .state     = nullptr,
    },
};

static CbProc kTractionProcs[] = {
    // rpm_to_speed — RPM + gear → ESC_SPEED_BUS bipolar.
    { .name      = "rpm_to_speed",
      .secInCh   = { AnalogComBusID::DRIVE_STATE_BUS, AnalogComBusID::GEAR },
      .fn        = sim_rpm_to_speed_fn,
      .cfg       = &kGearCfg,
      .state     = nullptr,
    },
};

static CbProc kSteeringProcs[] = {
    // bypass — DIRECT_DRIVE HIGH → claim (raw steering bypasses ramp).
    { .name    = "bypass",
      .secInCh = { DigitalComBusID::DIRECT_DRIVE },
      .fn      = sim_bypass_fn,
      .cfg     = nullptr,
      .state   = nullptr,
    },
    // ramp — progressive inertia.
    { .name  = "ramp",
      .fn    = sim_ramp_fn,
      .cfg   = &kSteerAsymRamp,
      .state = &gSteerRampState,
    },
};

static CbProc kDumpProcs[] = {
    // bypass — DIRECT_DRIVE HIGH → claim (raw dump bypasses ramp).
    { .name    = "bypass",
      .secInCh = { DigitalComBusID::DIRECT_DRIVE },
      .fn      = sim_bypass_fn,
      .cfg     = nullptr,
      .state   = nullptr,
    },
    // ramp — progressive asymmetric inertia.
    { .name  = "ramp",
      .fn    = sim_ramp_fn,
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
    .optOutCh   = AnalogComBusID::RPM_BUS,
    .procs      = kThrottleProcs,
    .procCount  = static_cast<uint8_t>(std::size(kThrottleProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "gear",
    .optInCh    = AnalogComBusID::RPM_BUS,
    .optOutCh   = AnalogComBusID::GEAR,
    .procs      = kGearProcs,
    .procCount  = static_cast<uint8_t>(std::size(kGearProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "traction",
    .optInCh    = AnalogComBusID::RPM_BUS,
    .optOutCh   = AnalogComBusID::ESC_SPEED_BUS,
    .procs      = kTractionProcs,
    .procCount  = static_cast<uint8_t>(std::size(kTractionProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "steering",
    .optInCh    = AnalogComBusID::STEERING_BUS,
    .optOutCh   = AnalogComBusID::STEERING_RAMPED_BUS,
    .procs      = kSteeringProcs,
    .procCount  = static_cast<uint8_t>(std::size(kSteeringProcs)),
    .chainOwner = kSimOwner,
  },

  { .name       = "dump",
    .optInCh    = AnalogComBusID::DUMP_BUS,
    .optOutCh   = AnalogComBusID::DUMP_RAMPED_BUS,
    .procs      = kDumpProcs,
    .procCount  = static_cast<uint8_t>(std::size(kDumpProcs)),
    .chainOwner = kSimOwner,
  },
};

#endif  // IS_MAINBOARD

// EOF dumper_truck_proc_config.cpp
