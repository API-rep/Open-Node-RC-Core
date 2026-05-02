/******************************************************************************
 * @file sound_presets.h
 * Ready-made sound device cfg presets — generic vehicle-class library.
 *
 * @details Each entry is a `static constexpr` cfg struct value.  Sound device
 *   profiles include this header and assign a preset address to the device's
 *   `cfg` pointer for any device that requires standard behaviour tuning.
 *
 *   Preset naming convention: `k<Class>_<Category>`.
 *   Threshold and range fields are expressed via `pctToCbus()` for readability
 *   (percentage of the ComBus half-range = 32767 units).
 *
 *   **Machine-specific fields** such as `HydPumpCfg::chanIDs` and
 *   `TriggerCfg::chan2ID` carry placeholder values in every preset.  They
 *   MUST be overridden by each machine profile to match the ComBus layout.
 *
 *   **Gate device** (`hdr.gateDevID`) is set to -1 (no gate) in all presets.
 *   Set it to the engine device array index in the machine profile.
 *****************************************************************************/
#pragma once

#include <core/system/sound/sound_device_cfg.h>
#include <core/system/combus/combus_res.h>    // pctToCbus


// =============================================================================
// 1. VOLUME RAMP — LIGHT HYDRAULIC CYLINDER  (benne dump, loader boom/bucket)
// =============================================================================

/**
 * @brief Volume ramp preset — light hydraulic cylinder (loader, dump body).
 *
 * @details Tight dead-band and quick ramp — matches a lightly-loaded single
 *   cylinder that reacts briskly to stick input (dump body lift, loader arm).
 *   Volume ceiling is 80 % to leave headroom for a pump overlay layer.
 *
 *   `hdr.gateDevID` defaults to -1 (no gate); set to the engine device index
 *   in the machine profile if engine-RPM gating is required.
 *
 *   Tuning guide:
 *   - `deadbandHalf` : increase to pctToCbus(6) for a sloppy / noisy stick.
 *   - `peakValue`    : decrease toward pctToCbus(40) for a snappier feel.
 *   - `rampPeriodMs` : increase to 12–16 ms to smooth out abrupt volume jumps.
 */
static constexpr HydRampCfg kHydRamp_Light {
    .hdr          = { .gateDevID = -1 },                              ///< No gate — override in profile.
    .deadbandHalf = static_cast<uint16_t>(pctToCbus(4u)),   ///< ±4 % dead zone around neutral.
    .peakValue    = static_cast<uint16_t>(pctToCbus(60u)),  ///< Full volume reached at 60 % stick.
    .maxVolume    = 80u,                                              ///< 80 % ceiling — headroom for pump overlay.
    .rampPeriodMs = 8u,                                               ///< ~125 ramp steps/s — brisk but smooth.
};


// =============================================================================
// 2. VOLUME RAMP — HEAVY HYDRAULIC CYLINDER  (excavator boom/bucket cylinder)
// =============================================================================

/**
 * @brief Volume ramp preset — heavy hydraulic cylinder (excavator, crane).
 *
 * @details Wider dead-band and slower ramp match a large, heavily-loaded
 *   cylinder (long boom, heavy bucket).  `maxVolume = 100` suits the higher
 *   acoustic load of excavator hydraulics.
 *
 *   `hdr.gateDevID` defaults to -1 (no gate); set to the engine device index
 *   in the profile for RPM-proportional gating.
 *
 *   Tuning guide:
 *   - `deadbandHalf` : decrease to pctToCbus(5) for a more sensitive response.
 *   - `peakValue`    : increase toward pctToCbus(90) for a very progressive feel.
 *   - `rampPeriodMs` : 16 ms gives a ~62 Hz ramp — smooth for slow hydraulics.
 */
static constexpr HydRampCfg kHydRamp_Heavy {
    .hdr          = { .gateDevID = -1 },                              ///< No gate — override in profile.
    .deadbandHalf = static_cast<uint16_t>(pctToCbus(8u)),   ///< ±8 % dead zone — absorbs backlash noise.
    .peakValue    = static_cast<uint16_t>(pctToCbus(80u)),  ///< Full volume reached at 80 % stick.
    .maxVolume    = 100u,                                             ///< 100 % ceiling for heavy machinery.
    .rampPeriodMs = 16u,                                              ///< ~62 ramp steps/s — smooth excavator feel.
};


// =============================================================================
// 3. MULTI-CYLINDER PUMP LOAD — EXCAVATOR 4-CYLINDER
// =============================================================================

/**
 * @brief Multi-cylinder pump load preset — 4-cylinder excavator.
 *
 * @details Four channels (e.g. bucket, dipper, boom, swing) each contribute
 *   up to 30 % to a combined load figure; max sum = 120 %, clamped to 100 %.
 *   Full load on any three simultaneous cylinders saturates the pump sound.
 *   `loadFeedback = true` drives engine RPM drop (up to 30 units at full load)
 *   and boosts diesel-knock volume proportionally.
 *
 *   **`chanIDs` MUST be overridden by the machine profile.**
 *   The values {0, 1, 2, 3} are placeholders — they must be replaced with the
 *   actual `AnalogComBusID` indices for the target vehicle's ComBus layout.
 *   Entries beyond `chanCount` (indices 4–7) are ignored by the interpreter.
 *
 *   `hdr.gateDevID` should be set to the engine device array index in the
 *   profile so that `loadFeedback` writes to the correct engine state.
 *
 *   Tuning guide:
 *   - `peakPerChan`  : set to 25 for exact 4-way sharing (4 × 25 = 100 %).
 *                      30 gives 20 % headroom — realistic when cylinders are
 *                      rarely all active simultaneously.
 *   - `maxLoadDrop`  : increase to 60 for engines with audible RPM sag.
 *   - `rampPeriodMs` : 12 ms — slightly slower than a cylinder, avoids flutter.
 */
static constexpr HydPumpCfg kHydPump_Excavator4cyl {
    .hdr          = { .gateDevID = -1 },                              ///< Set to engine device index in profile.
    .chanCount    = 4u,
    .chanIDs      = { 0u, 1u, 2u, 3u, 0u, 0u, 0u, 0u },             ///< Placeholder — MUST override in profile.
    .peakPerChan  = { 30u, 30u, 30u, 30u, 0u, 0u, 0u, 0u },         ///< 30 % each — clamped to 100 % total.
    .deadbandHalf = static_cast<uint16_t>(pctToCbus(4u)),   ///< Shared ±4 % dead zone across all channels.
    .loadFeedback = true,
    .maxLoadDrop  = 30u,                                              ///< ≈30 RPM units drop at full 4-cyl load.
    .rampPeriodMs = 12u,                                              ///< ~83 ramp steps/s — avoids pump flutter.
};


// =============================================================================
// 4. DELTA TRIGGER — FAST RATTLE  (bucket/track rattle on fast stick input)
// =============================================================================

/**
 * @brief Delta trigger preset — fast rattle on stick movement.
 *
 * @details Fires when a ComBus analog channel changes by ≥ 10 % of the
 *   half-range in one frame (~4 ms at 250 Hz).  Suitable for bucket rattle
 *   or track rattle triggered by a brisk deliberate stick motion.
 *
 *   `chan2ID = kNoChan` (single-channel monitoring by default).  Assign a
 *   second channel in the profile to monitor two axes simultaneously
 *   (e.g. boom + dipper).
 *
 *   `minGateVolume = 20` — engine must be at ≥ 20 % volume (i.e. already
 *   running) before the trigger is allowed.
 *
 *   `hdr.gateDevID` MUST be set to the engine device array index in the
 *   profile; the gate volume check is meaningless without a valid gate device.
 *
 *   Tuning guide:
 *   - `deltaThreshold` : increase to pctToCbus(15) for sharper-only triggers;
 *                        decrease to pctToCbus(6) to catch gentler movements.
 *   - `minGateVolume`  : set to 0 to allow rattle even at engine idle.
 */
static constexpr TriggerCfg kTrigger_RattleFast {
    .hdr            = { .gateDevID = -1 },                              ///< Set to engine device index in profile.
    .deltaThreshold = static_cast<uint16_t>(pctToCbus(10u)), ///< Fire at ≥ 10 % half-range delta / frame.
    .chan2ID         = TriggerCfg::kNoChan,                             ///< Single channel — override in profile for dual.
    .minGateVolume   = 20u,                                             ///< Engine ≥ 20 % volume required to trigger.
};


// =============================================================================
// 5. DELTA TRIGGER — CLANK  (sharp impact: end-stop, bucket ground hit)
// =============================================================================

/**
 * @brief Delta trigger preset — sharp mechanical clank (end-stop, ground hit).
 *
 * @details Fires only on a very fast stick input (≥ 25 % of the half-range
 *   in one frame), corresponding to a deliberate hard slam onto an end-stop
 *   or a heavy bucket-to-ground impact.
 *
 *   `minGateVolume = 10` — allows the clank even at near-idle engine state
 *   since a mechanical impact is independent of engine load.
 *
 *   `hdr.gateDevID` MUST be set to the engine device array index in the
 *   profile; the gate volume check is meaningless without a valid gate device.
 *
 *   Tuning guide:
 *   - `deltaThreshold` : decrease toward pctToCbus(15) for more sensitive clanks.
 *   - `minGateVolume`  : set to 0 to allow clank even when engine is fully off.
 */
static constexpr TriggerCfg kTrigger_Clank {
    .hdr            = { .gateDevID = -1 },                              ///< Set to engine device index in profile.
    .deltaThreshold = static_cast<uint16_t>(pctToCbus(25u)), ///< Fire at ≥ 25 % half-range delta / frame.
    .chan2ID         = TriggerCfg::kNoChan,                             ///< Single channel — override in profile for dual.
    .minGateVolume   = 10u,                                             ///< Near-idle allowed — mechanical timing.
};

// EOF sound_presets.h
