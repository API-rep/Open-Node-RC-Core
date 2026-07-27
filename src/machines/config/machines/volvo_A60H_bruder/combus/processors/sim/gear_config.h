/*!****************************************************************************
 * @file  gear_config.h
 * @brief Dumper truck — SIM gear chain: cfg, states, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     GearProcCfg, GearFsmState, GearDampState, CbProc, CbBypassCfg,
 *     cb_bypass_fn, gear_ratio_inv_fn, gear_upshift_rpm_fade_fn, gear_fsm_fn,
 *     gear_dyn_ramp_fn, gear_upshift_damp_fn, AnalogComBusID, DigitalComBusID,
 *     kGearShift_VolvoD16J, gTractionRampDyn (from throttle_config.h).
 *   kGearCfg defined here — sim_traction_config.h must be included after.
 *
 *   Chain function: ESC_RPM_BUS (wheel_speed seed) → automatic gear selection →
 *     GEAR + ESC_RPM_BUS overwritten to engine_rpm + GEAR_SHIFTING.
 *   In:  ESC_RPM_BUS (wheel_speed), SUBGEAR_BUS, DRIVE_STATE_BUS, DIRECT_DRIVE.
 *   Out: GEAR (final gear), ESC_RPM_BUS (engine_rpm), GEAR_SHIFTING (machine-local).
 *
 *   gear-inv-ratio: wheel_speed * 1000 / gearRatio[prevGear] → engine_rpm written to
 *     ESC_RPM_BUS. rpm-fade overrides it. Gear=0 (uninit) → passthrough. 1-cycle
 *     latency on gGearFsmState.gear (same as gear_dyn_ramp_fn).
 *   rpm-fade: sole authoritative writer of ESC_RPM_BUS in this chain. Active damp window:
 *     interpolates rpmAtShift → natural RPM over upshiftDampMs. Pass-through otherwise.
 *     gGearDampState shared with upshift-damp.
 *   gear-ramp: writes gTractionRampDyn.rampTimeMs after gear selection; THROTTLE chain
 *     reads it at next cycle (1-cycle latency — avoids circular dependency).
 *   upshift-damp: observer — does not modify `value`. Writes extAccelSteps to freeze
 *     traction ramp for upshiftDampMs. Commits GEAR_SHIFTING=1 during damp window.
 *     Automatically disabled when gear chain is off (no stale side-effects).
 *     @todo winter 2026: promote GEAR_SHIFTING to WIRE region for sound node access.
 *   Claim cascade — priority: SubGear > Manual > Direct > Auto (FSM).
 *     manual-claim disabled: MANUAL_GEAR_SET not yet written by INPUT chain —
 *     uninitialised channel causes random claims → saccades.
 ******************************************************************************/
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

static constexpr GearProcCfg kGearCfg {
    .profile = &kGearShift_VolvoD16J,
};

static constexpr CbBypassCfg kSubGearClaimBypass {
    .forceValue = 1u,  ///< Force GEAR = 1 when sub-gear mode active.
                       ///< gear_ratio_fn applies gearRatio[0] ‰ to limit wheel speed;
                       ///< gear_subgear_cap_fn applies an additional % cap.
};

static constexpr CbBypassCfg kDirectDriveGearBypass {
    .forceValue = 1u,  // Force GEAR = 1 on DIRECT_DRIVE claim.
};


// =============================================================================
// 2. STATE
// =============================================================================

static GearFsmState  gGearFsmState  {};
static GearDampState gGearDampState {};


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Gear chain proc array.
 * @details Steps:
 *   1  gear-inv-ratio : wheel_speed * 1000 / gearRatio[prevGear] → engine_rpm.
 *                       Writes ESC_RPM_BUS (initial write); rpm-fade is the final writer.
 *                       Passthrough at gear=0. 1-cycle latency on gGearFsmState.gear.
 *   2  rpm-fade       : sole ESC_RPM_BUS writer. During upshift damp: interpolates
 *                       rpmAtShift → natural RPM over upshiftDampMs. Pass-through otherwise.
 *   5  Claim cascade  : first claim wins, later procs skipped.
 *                       Priority: SubGear > Manual > Direct > Auto (FSM).
 *   5.1  subgear-claim : SUBGEAR_BUS != 0 → force gear=1, claim chain.
 *  (5.2) manual-claim  : disabled — see @details.
 *   5.3  direct-claim  : DIRECT_DRIVE HIGH → force gear=1, claim chain.
 *   6  gear-fsm       : hysteresis FSM on RPM thresholds + DRIVE_STATE.
 *   7  gear-ramp      : writes gTractionRampDyn.rampTimeMs for new gear.
 *   8  upshift-damp   : observer — extAccelSteps + GEAR_SHIFTING=1 during damp window.
 */

static CbProc kGearProcs[] = {
      // 1. gear-inv-ratio — wheel_speed → engine_rpm; side-writes ESC_RPM_BUS.
    { .name    = "gear-inv-ratio",
      .outCh   = AnalogComBusID::ESC_RPM_BUS,
      .fn      = gear_ratio_inv_fn,
      .cfg     = &kGearCfg,
      .state   = &gGearFsmState,   // read-only; gear_fsm_fn writes it later in chain
    },
      // 2. rpm-fade — ESC_RPM_BUS interpolation during upshift damp; pass-through otherwise.
    { .name    = "rpm-fade",
      .outCh   = AnalogComBusID::ESC_RPM_BUS,
      .fn      = gear_upshift_rpm_fade_fn,
      .cfg     = &kGearCfg,
      .state   = &gGearDampState,
    },
      // 5.1. subgear-claim — SUBGEAR_BUS != 0 → force gear=1, claim.
    { .name      = "subgear-claim",
      .inCh      = AnalogComBusID::SUBGEAR_BUS,
      .fn        = cb_bypass_fn,
      .cfg       = &kSubGearClaimBypass,
    },

      // (5.2) manual-claim — MANUAL_GEAR_SET HIGH → passthrough, claim. (disabled — see @details)
    /*
    { .name      = "manual-claim",
      .inCh      = { DigitalComBusID::MANUAL_GEAR_SET },
      .fn        = cb_bypass_fn,
    },
    */
      // 5.3. direct-claim — DIRECT_DRIVE HIGH → force gear=1, claim.
    { .name      = "direct-claim",
      .inCh      = DigitalComBusID::DIRECT_DRIVE,
      .fn        = cb_bypass_fn,
      .cfg       = &kDirectDriveGearBypass,
    },
      // 6. gear-fsm — RPM + DRIVE_STATE → automatic gear (no prior claim).
    { .name      = "gear-fsm",
      .inCh      = AnalogComBusID::DRIVE_STATE_BUS,
      .fn        = gear_fsm_fn,
      .cfg       = &kGearCfg,
      .state     = &gGearFsmState,
    },
      // 7. gear-ramp — updates gTractionRampDyn.rampTimeMs for the selected gear.
    { .name      = "gear-ramp",
      .fn        = gear_dyn_ramp_fn,
      .cfg       = &kGearCfg,
      .dynCfg    = &gTractionRampDyn,
    },
      // 8. upshift-damp — observer: freeze ramp + signal GEAR_SHIFTING on upshift.
    { .name    = "upshift-damp",
      .outCh   = DigitalComBusID::GEAR_SHIFTING,
      .fn      = gear_upshift_damp_fn,
      .cfg     = &kGearCfg,
      .dynCfg  = &gTractionRampDyn,
      .state   = &gGearDampState,
    },
};

// EOF gear_config.h
