/*!****************************************************************************
 * @file  throttle_config.h
 * @brief Dumper truck — SIM throttle chain: cfg, states, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbRampCfg, CbRampState, CbBrakeCfg, CbBrakeState, CbCenterCfg,
 *     CbDirCfg, CbScaleCfg, CbProc, cb_sym_ramp_fn, cb_dir_fn, cb_brake_fn,
 *     cb_rev_brake_fn, cb_center_fn, cb_abs_fn, cb_scale_fn, cb_bypass_fn,
 *     AnalogComBusID, DigitalComBusID, CbusNeutral, pctToCbus,
 *     kVolvoD16J_steps, kGearShift_VolvoD16J.
 *   Declaration order: kTractionRamp → gTractionRampDyn → kBrakeCfg
 *     (kBrakeCfg references gTractionRampDyn and kTractionRamp.neutralBand).
 *   gTractionRampDyn defined here — GEAR chain's gear-ramp proc mutates it on each gear change.
 *
 *   Chain function: THROTTLE_BUS → ramp → dir → b-in → brake → center → abs → scale → ESC_RPM_BUS.
 *   In:  THROTTLE_BUS (raw stick), BRAKE_BUS (pedal), DIRECT_DRIVE.
 *   Out: ESC_RPM_BUS (wheel_speed), DRIVE_STATE_BUS (dir side-effect), BRAKE_BUS (brake merge).
 *
 *   ramp: heavy-truck inertia; rampTimeMs from gTractionRampDyn — updated by GEAR chain (1-cycle lag).
 *   dir: sole writer of DRIVE_STATE_BUS; value passes through unchanged (center/abs follow).
 *   b-in: observer — caches BRAKE_BUS into gBrakeState.brakeVal each cycle.
 *   brake: merges reverse-brake (opposing stick > neutralBand) with pedal input;
 *     side-writes BRAKE_BUS and scales dynRampCfg.brakeSteps proportionally to brake force.
 *   bypass: DIRECT_DRIVE HIGH → claim; raw magnitude bypasses gear-ratio in TRACTION chain.
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  Traction — heavy truck inertia.
static constexpr CbRampCfg kTractionRamp {
    .rampTimeMs  = 50u,
    .accelSteps  = pctToCbus(1),
    .brakeSteps  = pctToCbus(1),   ///< Near-zero natural decel — vehicle coasts freely; active braking via cb_rev_brake_fn.
    .neutralBand = pctToCbus(8),   ///< 8 % deadzone — absorbs PS4 stick noise (±~0.4%) at neutral,
                                   ///<   preventing cb_dir_fn from oscillating FWD↔REV at rest.
                                   ///<   Also used as revBrakeDeadBand in kBrakeCfg.
};

  ///  Traction ramp — RAM copy, mutated by gear_dyn_ramp_fn on each gear change.
static CbRampCfg gTractionRampDyn = kTractionRamp;

static constexpr CbCenterCfg kThrottleCenter {};
static constexpr CbDirCfg    kThrottleDirCfg {};

static constexpr CbScaleCfg kThrottleScale {
    .inMax  = CbusNeutral,
    .outMax = static_cast<uint16_t>(kVolvoD16J_steps[std::size(kVolvoD16J_steps) - 1u].upShift),
    // outMin not set (= 0): wheel_speed = 0 at neutral stick in wheel-speed-primary model.
    // Sound node handles idle RPM internally.
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


// =============================================================================
// 2. STATE
// =============================================================================

static CbRampState  gTractionRampState {};
static CbBrakeState gBrakeState        {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Throttle chain proc array.
 * @details Steps:
 *   1  ramp   : THROTTLE_BUS inertia; rampTimeMs from gTractionRampDyn (gear-updated, 1-cycle lag).
 *   2  dir    : side-writes DRIVE_STATE_BUS from ramped sign; value unchanged.
 *   3  b-in   : observer — reads BRAKE_BUS each cycle, caches into gBrakeState.brakeVal.
 *   4  brake  : merges revBrake (opposing stick > neutralBand) + pedal input;
 *               side-writes BRAKE_BUS and scales dynRampCfg.brakeSteps to brake force.
 *   5  center : signed deviation from CbusNeutral.
 *   6  abs    : absolute value; sign captured by dir proc.
 *   7  scale  : [0..CbusNeutral] → [0..maxRpm]; outMax = kVolvoD16J top upShift threshold.
 *   8  bypass : DIRECT_DRIVE HIGH → claim; magnitude skips gear-ratio division in TRACTION chain.
 */

static CbProc kThrottleProcs[] = {
      // 1. ramp — inertia on THROTTLE_BUS; rampTimeMs from gTractionRampDyn.
    { .name    = "ramp",
      .fn      = cb_sym_ramp_fn,
      .cfg     = &kTractionRamp,
      .dynCfg  = &gTractionRampDyn,
      .state   = &gTractionRampState,
    },
      // 2. dir — side-writes DRIVE_STATE_BUS; value passes through.
    { .name   = "dir",
      .outCh  = AnalogComBusID::DRIVE_STATE_BUS,
      .fn     = cb_dir_fn,
      .cfg    = &kThrottleDirCfg,
    },
      // 3. b-in — observer: caches BRAKE_BUS into gBrakeState.brakeVal each cycle.
    { .name  = "b-in",
      .inCh  = AnalogComBusID::BRAKE_BUS,
      .fn    = cb_brake_fn,
      .state = &gBrakeState,
    },
      // 4. brake — merges revBrake + pedal; side-writes BRAKE_BUS + brakeSteps.
    { .name  = "brake",
      .inCh  = AnalogComBusID::THROTTLE_BUS,
      .outCh = AnalogComBusID::BRAKE_BUS,
      .fn    = cb_rev_brake_fn,
      .cfg   = &kBrakeCfg,
      .state = &gBrakeState,
    },
      // 5. center — signed deviation from CbusNeutral.
    { .name  = "center",
      .fn    = cb_center_fn,
      .cfg   = &kThrottleCenter,
    },
      // 6. abs — absolute value; sign captured by dir proc.
    { .name  = "abs",
      .fn    = cb_abs_fn,
    },
      // 7. scale — [0..CbusNeutral] → [0..maxRpm].
    { .name  = "scale",
      .fn    = cb_scale_fn,
      .cfg   = &kThrottleScale,
    },
      // 8. bypass — DIRECT_DRIVE HIGH → claim; magnitude skips gear-ratio in TRACTION chain.
    { .name  = "bypass",
      .inCh  = DigitalComBusID::DIRECT_DRIVE,
      .fn    = cb_bypass_fn,
    },
};

// EOF throttle_config.h
