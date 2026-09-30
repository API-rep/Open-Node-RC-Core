/*!****************************************************************************
 * @file  runlevel_config.h
 * @brief Dumper truck — INPUT runlevel chain: cfg, state, proc array.
 *
 * @details Private include — included only from proc_config.cpp.
 *   Context provided by umbrella (do not include standalone):
 *     CbRunlevelCfg, CbRunlevelOnceState, CbProc, cb_runlevel_fn,
 *     cb_runlevel_once_fn, AnalogComBusID, DigitalComBusID, RunLevel, comBus.
 *
 *   Chain function: RUNLEVEL → [failsafe, runlevel] → RUNLEVEL.
 *   In:  RUNLEVEL (current RunLevel value, seeded into pipeline value).
 *   Out: RUNLEVEL (committed after pipeline).
 *
 *   1  failsafe:  inCh = FAILSAFE.  CONTINUOUS (cb_runlevel_fn).
 *                 When FAILSAFE=true → write RUNLEVEL=FAILSAFE (dedicated value, see machines_defs.h)
 *                 and claim the chain (blocks runlevel proc).
 *                 When FAILSAFE=false → no write, no claim.
 *                 The dedicated RunLevel::FAILSAFE value triggers the
 *                 hardware safety reaction (stop/sleep/disable drivers)
 *                 in main.cpp's switch(curRunLevel).
 *
 *   1b remote_link: inCh = REMOTE_LINK_LOST.  EDGE-TRIGGERED (cb_runlevel_once_fn).
 *                 chantier 12.5: REMOTE_LINK_LOST is the OR-aggregated health of all
 *                 remote input sources (PS4_BT, UART, …) computed by the
 *                 remote_link_fallback_chain.  On its rising edge (0→1, link
 *                 just dropped), force RUNLEVEL=IDLE and claim the chain (so
 *                 the KEY_ACTIVE proc cannot immediately override).  On the
 *                 falling edge (1→0, link recovered), no write — recovery is
 *                 manual via the operator (KEY → STARTING).
 *                 This proc sits RIGHT AFTER failsafe in the priority order,
 *                 so a FAILSAFE condition always wins.
 *                 Gated by `-D HAS_REMOTE_LINK_LOST_FALLBACK` (mirrors the
 *                 umbrella pattern of the aggregator chain itself — no
 *                 chain → no edge-triggered proc needed).
 *
 *   2  runlevel:  inCh = KEY_ACTIVE.  EDGE-TRIGGERED (cb_runlevel_once_fn).
 *                 On KEY_ACTIVE 0→1 → write RUNLEVEL=STARTING (once).
 *                 On KEY_ACTIVE 1→0 → write RUNLEVEL=TURNING_OFF (once).
 *                 No claim (last proc in chain).
 *                 The edge-triggered variant is required so the FSM in
 *                 main.cpp can progress STARTING→RUNNING without being
 *                 overwritten back to STARTING on every cycle.
 *
 *   The chain uses read-modify-write: the runner seeds `value` from RUNLEVEL,
 *   each proc may overwrite it, and the final value is committed back to RUNLEVEL.
 *******************************************************************************
 */
#pragma once


// =============================================================================
// 1. CONFIG
// =============================================================================

  ///  failsafe — FAILSAFE=true → force RUNLEVEL=FAILSAFE (dedicated value, see machines_defs.h), claim chain.
static constexpr CbRunlevelCfg kFailsafeRunlevelCfg {
    .high  = RunLevel::FAILSAFE,    ///< FAILSAFE=true → write FAILSAFE (dedicated value — triggers hardware safety in main.cpp).
    .low   = std::nullopt,          ///< FAILSAFE=false → no write.
    .claim = true,                  ///< Block runlevel proc when failsafe is active.
};

  ///  runlevel — KEY_ACTIVE drives STARTING/TURNING_OFF on edge transitions.
static constexpr CbRunlevelCfg kRunlevelCfg {
    .high  = RunLevel::STARTING,    ///< KEY_ACTIVE 0→1 → write STARTING.
    .low   = RunLevel::TURNING_OFF, ///< KEY_ACTIVE 1→0 → write TURNING_OFF.
    .claim = false,                 ///< Last proc — nothing to protect behind.
};

#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)
  ///  remote_link — chantier 12.5: REMOTE_LINK_LOST 0→1 → force RUNLEVEL=IDLE + claim.
  ///  Rising edge only (edge-triggered).  Falling edge = manual recovery via KEY.
  ///  Sits AFTER failsafe so a FAILSAFE condition always wins.
static constexpr CbRunlevelCfg kRemoteLinkRunlevelCfg {
    .high  = RunLevel::IDLE,        ///< REMOTE_LINK_LOST 0→1 → write IDLE.
    .low   = RunLevel::STARTING,    ///< REMOTE_LINK_LOST 1→0 → write STARTING (recovery — REMOTE_LINK_LOST engenders IDLE mode, no cold-start).
    .claim = true,                  ///< Block KEY_ACTIVE proc on link loss.
};
#endif  // HAS_REMOTE_LINK_LOST_FALLBACK


// =============================================================================
// 2. STATE
// =============================================================================

  //  runlevel edge-triggered state — prevValue initialised to 0 (no prior lever state).
static CbRunlevelOnceState gRunlevelOnceState {};

#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)
  //  remote_link edge-triggered state — separate instance (one per proc).
static CbRunlevelOnceState gRemoteLinkOnceState {};
#endif


// =============================================================================
// 3. PROC ARRAY
// =============================================================================

/**
 * @brief  Runlevel chain proc array.
 * @details Steps (chantier 12.5 new design):
 *   1  failsafe    : CONTINUOUS — FAILSAFE=true → RUNLEVEL=FAILSAFE + claim (highest priority).
 *   1b remote_link : EDGE-TRIGGERED — REMOTE_LINK_LOST 0→1 → RUNLEVEL=IDLE + claim.
 *   2  runlevel    : EDGE-TRIGGERED — KEY_ACTIVE transition → RUNLEVEL=STARTING or TURNING_OFF.
 *
 *   Priority: failsafe > remote_link > runlevel.  Failsafe always wins
 *   (the safety condition overrides everything).  remote_link claims when
 *   active so KEY_ACTIVE cannot immediately recover — the operator must
 *   explicitly reset (KEY → STARTING) once the link is back.
 */
static CbProc kRunlevelProcs[] = {
      // 1. failsafe — CONTINUOUS: FAILSAFE=true → force RunLevel::FAILSAFE (dedicated) + claim chain.
    { .name  = "failsafe",
      .inCh  = DigitalComBusID::FAILSAFE,
      .fn    = cb_runlevel_fn,
      .cfg   = &kFailsafeRunlevelCfg,
      .state = nullptr,
    },
#if defined(HAS_REMOTE_LINK_LOST_FALLBACK)
      // 1b. remote_link — EDGE-TRIGGERED: REMOTE_LINK_LOST 0→1 → RUNLEVEL=IDLE + claim.
      //     Sits right after failsafe so it benefits from failsafe's claim
      //     (no need to check FAILSAFE here — chain is already claimed).
      //     Gated by HAS_REMOTE_LINK_LOST_FALLBACK — without the aggregator
      //     chain, the proc is meaningless (REMOTE_LINK_LOST is never latched).
    { .name  = "remote_link",
      .inCh  = DigitalComBusID::REMOTE_LINK_LOST,
      .fn    = cb_runlevel_once_fn,
      .cfg   = &kRemoteLinkRunlevelCfg,
      .state = &gRemoteLinkOnceState,
    },
#endif
      // 2. runlevel — EDGE-TRIGGERED: KEY_ACTIVE transition → STARTING/TURNING_OFF.
    { .name  = "runlevel",
      .inCh  = DigitalComBusID::KEY_ACTIVE,
      .fn    = cb_runlevel_once_fn,
      .cfg   = &kRunlevelCfg,
      .state = &gRunlevelOnceState,
    },
};

// EOF runlevel_config.h