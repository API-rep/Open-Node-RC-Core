// [SANDBOX — migration reference only, not compiled]
// Original: src/core/config/hw/esc/esc_presets.h
// Delete migrated presets as they are rebuilt in motion_presets.h.

/******************************************************************************
 * @file esc_presets.h
 * @brief Legacy EscInertiaConfig presets — to be rebuilt as layered MotionConfig
 *****************************************************************************/

// =============================================================================
// 1. TRACTION PRESET  →  rebuild as MotionConfig (MotionHw + MotionRamp + MotionGear + MotionFsm)
// =============================================================================

// kMotion_Traction_Truck
// .mode             = MotionMode::TRACTION_FSM
// .cbusNeutral      = 32767u        → kCbusNeutral
// .cbusMax          = 65535u        → CbusMaxVal
// .cbusMin          = 0u            → kCbusMin
// .cbusMaxNeutral   = 32767u        → MotionBand.maxNeutral  (no dead-band: = neutral)
// .cbusMinNeutral   = 32767u        → MotionBand.minNeutral  (no dead-band: = neutral)
// .cbusMaxLimit     = 65535u        → MotionMargin.maxVal
// .cbusMinLimit     = 0u            → MotionMargin.minVal
// .escMax           = 65535u        → MotionHw.maxHwVal
// .escMin           = 0u            → MotionHw.minHwVal
// .escMaxNeutral    = 32767u        → MotionBand (hw side)
// .escMinNeutral    = 32767u        → MotionBand (hw side)
// .rampTimeFirstMs  = 20u           → MotionGear.rampTimeFirstMs
// .rampTimeSecondMs = 55u           → MotionGear.rampTimeSecondMs
// .rampTimeThirdMs  = 80u           → MotionGear.rampTimeThirdMs
// .crawlerRampTimeMs = 2u           → MotionGear.crawlerRampTimeMs
// .brakeSteps       = 655u  (~1%)   → MotionFsm.brakeSteps
// .accelSteps       = 327u  (~0.5%) → MotionFsm.accelSteps
// .brakeMargin      = 1310u (~2%)   → MotionFsm.brakeMargin
// .globalAccelPct   = 100u          → MotionGear.globalAccelPct
// .lowRangePct      = 50u           → MotionGear.lowRangePct
// .autoRevAccelPct  = 100u          → MotionGear.autoRevAccelPct
// .linearizeFn      = nullptr       → MotionFsm.linearizeFn
// .comBus / .owner  → MotionConfig.comBus / .owner


// =============================================================================
// 2. HYDRAULIC PRESET  →  rebuild as MotionConfig (MotionHw + MotionRamp only)
// =============================================================================

// kMotion_Hydraulic_Slow
// .mode             = MotionMode::RAMP_SIMPLE
// .rampTimeFirstMs  = 200u          → MotionRamp.rampTimeMs
// .brakeSteps       = 327u          → MotionRamp.accelSteps (RAMP_SIMPLE uses accelSteps for both)
// .accelSteps       = 327u          → MotionRamp.accelSteps
// .brakeMargin      = 0u            → (not used for RAMP_SIMPLE)
// All range fields   = full / no dead-band / no limit  → MotionHw defaults


// =============================================================================
// 3. STEERING PRESET  →  rebuild as MotionConfig (MotionHw + MotionRamp only)
// =============================================================================

// kMotion_Steer
// .mode             = MotionMode::RAMP_SIMPLE
// .rampTimeFirstMs  = 50u           → MotionRamp.rampTimeMs
// .brakeSteps       = 1310u (~2%)   → MotionRamp.accelSteps (fast centering)
// .accelSteps       = 1310u (~2%)   → MotionRamp.accelSteps
// .brakeMargin      = 0u
// All range fields   = full / no dead-band / no limit  → MotionHw defaults

// EOF sandbox/esc_presets.h
