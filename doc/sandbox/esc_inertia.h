// [SANDBOX — migration reference only, not compiled]
// Original: src/core/system/hw/esc_inertia.h
// Delete migrated sections as they are covered by the new motion architecture.

/******************************************************************************
 * @file esc_inertia.h
 * @brief Stateless vehicle-inertia FSM for ESC output.
 *****************************************************************************/
#pragma once

#include <cstdint>
#include <struct/esc_inertia_struct.h>


// =============================================================================
// 1. INPUTS  →  migrates to MotionInputs in motion.h
// =============================================================================

struct EscInertiaInputs {
    bool     engineRunning;
    bool     neutralGear;
    bool     failSafe;
    bool     batteryProtection;
    bool     crawlerMode;
    bool     lowRange;
    uint8_t  selectedGear;
    bool     automatic;
    bool     doubleClutch;
    bool     shiftingAutoThrottle;
    bool     gearUpShiftingPulse;
    bool     gearDownShiftingPulse;
    bool     gearUpShiftingInProgress;
    bool     gearDownShiftingInProgress;
    uint16_t currentThrottle;       // 0–500 from ENGINE_RPM_BUS
    uint16_t speedLimit;            // 0–500 top speed
    uint16_t overrideRampTimeMs;    // 0 = auto; non-zero = override
};


// =============================================================================
// 2. API  →  migrates to motion_update() in motion.h
// =============================================================================

void esc_inertia_update(uint16_t                cbusVal,
                        const EscInertiaInputs& inputs,
                        const EscInertiaConfig* cfg,
                        EscInertiaRuntime&      rt,
                        EscInertiaState*        out);

// EOF sandbox/esc_inertia.h
