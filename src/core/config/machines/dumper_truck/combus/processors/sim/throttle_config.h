/*!****************************************************************************
 * @file  throttle_config.h
 * @brief Dumper truck — SIM throttle chain: cfg, states, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbRampCfg, CbRampState, CbBrakeCfg, CbBrakeState, CbCenterCfg,
 *     CbDirCfg, CbScaleCfg, CbCruiseCfg, CbCruiseState, CbProc,
 *     cb_sym_ramp_fn, cb_dir_fn, cb_brake_fn, cb_rev_brake_fn,
 *     cb_center_fn, cb_abs_fn, cb_scale_fn, cb_bypass_fn, cb_cruise_fn,
 *     AnalogComBusID, DigitalComBusID, CbusNeutral, pctToCbus,
 *     kVolvoD16J_steps, kGearShift_VolvoD16J.
 *   Declaration order: kTractionRamp → gTractionRampDyn → kBrakeCfg → gTractionRampState → kCruiseCfg
 *     (kBrakeCfg references gTractionRampDyn; kCruiseCfg references gTractionRampDyn + gTractionRampState).
 *
 *   Chain function: THROTTLE_BUS → bypass → ramp → dir → b-in → brake → cruise_sync → cruise_upd → cruise → center → abs → scale → ESC_RPM_BUS.
 *   In direct mode bypass claims at pos 1; ESC_RPM_BUS = raw THROTTLE_BUS (bipolar, no inertia).
 *   In:  THROTTLE_BUS (raw stick), BRAKE_BUS (pedal), DIRECT_DRIVE, SUBGEAR_BUS (holdSpeed),
 *        CRUISE_ACTIVE (normal cruise toggle), CRUISE_UPDATE_BTN (L3 speed update).
 *   Out: ESC_RPM_BUS (wheel_speed), DRIVE_STATE_BUS (dir side-effect), BRAKE_BUS (brake merge).
 *
 *   bypass: DIRECT_DRIVE HIGH → claim; entire chain skipped → raw THROTTLE_BUS written to ESC_RPM_BUS.
 *   ramp: heavy-truck inertia; rampTimeMs from gTractionRampDyn — updated by GEAR chain (1-cycle lag).
 *   dir: sole writer of DRIVE_STATE_BUS; value passes through unchanged (center/abs follow).
 *   b-in: observer — caches BRAKE_BUS into gBrakeState.brakeVal each cycle.
 *   brake: merges reverse-brake (opposing stick > neutralBand) with pedal input;
 *     side-writes BRAKE_BUS and scales dynRampCfg.brakeSteps proportionally to brake force.
 *   cruise_sync (5.3): reads CRUISE_ACTIVE → gCruiseState.active (passthrough).
 *   cruise_upd  (5.4): L3 pressed → gCruiseState.updateReq = true (passthrough).
 *   cruise      (5.5): holdSpeed mode (SUBGEAR_BUS) + normal mode (gCruiseState.active);
 *     floor hold + adaptive watermark; ramp currentPos synced on deactivation.

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
    .maxBrakeStep      = pctToCbus(100),    ///< extBrakeSteps ceiling at full brake — 4× increase compensates 40 % deadband.
    .brakeScalePct     = 20u,              ///< Brake-input max deflection = 30 % of maxBrake.
    .revBrakeScalePct  = 200u,             ///< Reverse-stick 2× amplification — tune on hardware.
    .revBrakeDeadBand  = pctToCbus(80),    ///< 80 % — deliberate reverse-stick zone only; prevents FW-BACK-FW
                                           ///<   near neutral from arming revBraking on abrupt stick release.
};

  ///  Cruise control — holdSpeed mode: auto-activated by SUBGEAR_BUS, floor hold + adaptive watermark.
static CbRampState   gTractionRampState {};  ///< Declared here — kCruiseCfg below holds &gTractionRampState.
static const CbCruiseCfg kCruiseCfg {
    .holdSpeed  = true,
    .dynRampCfg = &gTractionRampDyn,    ///< Read extBrakeSteps for braking detection.
    .rampState  = &gTractionRampState,  ///< Sync currentPos on deactivation.
};


// =============================================================================
// 2. STATE
// =============================================================================

static CbBrakeState  gBrakeState   {};
static CbCruiseState gCruiseState  {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Throttle chain proc array.
 * @details Steps:
 *   1  bypass     : DIRECT_DRIVE HIGH → claim; entire chain skipped → raw THROTTLE_BUS to ESC_RPM_BUS.
 *   2  ramp       : THROTTLE_BUS inertia; rampTimeMs from gTractionRampDyn (gear-updated, 1-cycle lag).
 *   3  dir        : side-writes DRIVE_STATE_BUS from ramped sign; value unchanged.
 *   4  b-in       : observer — reads BRAKE_BUS each cycle, caches into gBrakeState.brakeVal.
 *   5  brake      : merges revBrake (opposing stick > neutralBand) + pedal input;
 *                   side-writes BRAKE_BUS and scales dynRampCfg.brakeSteps to brake force.
 *   5.3 cruise_sync: CRUISE_ACTIVE → gCruiseState.active (normal cruise ComBus sync, passthrough).
 *   5.4 cruise_upd : CRUISE_UPDATE_BTN → gCruiseState.updateReq = true when L3 pressed (passthrough).
 *   5.5 cruise    : holdSpeed (SUBGEAR_BUS) + normal (gCruiseState.active); floor hold + watermark.
 *   6  center     : signed deviation from CbusNeutral.
 *   7  abs        : absolute value; sign captured by dir proc.
 *   8  scale      : [0..CbusNeutral] → [0..maxRpm]; outMax = kVolvoD16J top upShift threshold.
 */

static CbProc kThrottleProcs[] = {
      // 1. bypass — DIRECT_DRIVE HIGH → claim; entire chain skipped → raw THROTTLE_BUS to ESC_RPM_BUS.
    { .name  = "bypass",
      .inCh  = DigitalComBusID::DIRECT_DRIVE,
      .fn    = cb_bypass_fn,
    },
      // 2. ramp — inertia on THROTTLE_BUS; rampTimeMs from gTractionRampDyn.
    { .name    = "ramp",
      .fn      = cb_sym_ramp_fn,
      .cfg     = &kTractionRamp,
      .dynCfg  = &gTractionRampDyn,
      .state   = &gTractionRampState,
    },
      // 3. dir — side-writes DRIVE_STATE_BUS; value passes through.
    { .name   = "dir",
      .outCh  = AnalogComBusID::DRIVE_STATE_BUS,
      .fn     = cb_dir_fn,
      .cfg    = &kThrottleDirCfg,
    },
      // 4. b-in — observer: caches BRAKE_BUS into gBrakeState.brakeVal each cycle.
    { .name  = "b-in",
      .inCh  = AnalogComBusID::BRAKE_BUS,
      .fn    = cb_brake_fn,
      .state = &gBrakeState,
    },
      // 5. brake — merges revBrake + pedal; side-writes BRAKE_BUS + brakeSteps.
    { .name  = "brake",
      .inCh  = AnalogComBusID::THROTTLE_BUS,
      .outCh = AnalogComBusID::BRAKE_BUS,
      .fn    = cb_rev_brake_fn,
      .cfg   = &kBrakeCfg,
      .state = &gBrakeState,
    },
      // 5.3. cruise_sync — CRUISE_ACTIVE → gCruiseState.active (normal cruise ComBus sync).
    { .name  = "cruise_sync",
      .inCh  = DigitalComBusID::CRUISE_ACTIVE,
      .fn    = cb_cruise_sync_fn,
      .state = &gCruiseState,
    },
      // 5.4. cruise_upd — L3 pressed → gCruiseState.updateReq = true (cruise speed update).
    { .name  = "cruise_upd",
      .inCh  = DigitalComBusID::CRUISE_UPDATE_BTN,
      .fn    = cb_cruise_upd_fn,
      .state = &gCruiseState,
    },
      // 5.5. cruise — holdSpeed: SUBGEAR_BUS != 0; normal: gCruiseState.active.
    { .name   = "cruise",
      .inCh   = AnalogComBusID::SUBGEAR_BUS,
      .fn     = cb_cruise_fn,
      .cfg    = &kCruiseCfg,
      .state  = &gCruiseState,
    },
      // 6. center — signed deviation from CbusNeutral.
    { .name  = "center",
      .fn    = cb_center_fn,
      .cfg   = &kThrottleCenter,
    },
      // 7. abs — absolute value; sign captured by dir proc.
    { .name  = "abs",
      .fn    = cb_abs_fn,
    },
      // 8. scale — [0..CbusNeutral] → [0..maxRpm]; outMax = kVolvoD16J top upShift threshold.
    { .name  = "scale",
      .fn    = cb_scale_fn,
      .cfg   = &kThrottleScale,
    },
};

// EOF throttle_config.h
