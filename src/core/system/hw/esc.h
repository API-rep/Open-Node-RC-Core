/******************************************************************************
 * @file esc.h
 * @brief Board-agnostic ESC output dispatch â€” init, calibrate, write.
 *
 * @details Manages a runtime-sized array of ESC output channels without
 *   exposing hardware objects to the caller.  Configuration is supplied at
 *   init time through a `DcDevice` array so machine code never needs direct
 *   access to `ServoCore` or `DcMotorCore`.
 *
 *   Activation: `-D ESC_OUTPUT_ENABLED` in build flags.
 *   All public functions compile to no-ops when the flag is absent.
 *
 *   Pulse range calibration is independent of hardware activation and is
 *   always available via `esc_calibrate()`.  This allows the inertia FSM
 *   (`esc_inertia`) to use correct range values even when no physical ESC
 *   is wired.
 *****************************************************************************/
#pragma once

#include <cstdint>
#include <struct/machines_struct.h>
#include "pin_reg.h"


// 1. PULSE RANGE CALIBRATION  (always available, independent of ESC_OUTPUT_ENABLED)
// =============================================================================

/**
 * @brief Compute the four ESC pulse-range calibration constants.
 *
 * @details Derives the hardware pulse range from three constexpr profile
 *   values.  The result is suitable for both the inertia FSM and for the
 *   signal mapping step that converts `escPulseWidthOut` â†’ 1000â€“2000 Âµs.
 *
 *   All parameters and outputs are expressed in 16-bit ComBus units (0â€“65535).
 *   The neutral point is always `EscNeutral = 32767`.
 *
 *   Formulas:
 *   @code
 *     outMax        = EscNeutral + halfSpan
 *     outMin        = EscNeutral - halfSpan + reversePlus
 *     outMaxNeutral = EscNeutral + deadBandHalf
 *     outMinNeutral = EscNeutral - deadBandHalf
 *   @endcode
 *
 * @param halfSpan       Forward/reverse half-range (0â€“32767).
 *                       E.g. 32767 for a full-range ESC, 26214 for a 80%-range ESC.
 * @param deadBandHalf   Half-width of the takeoff dead-band (0â€“32767, typically 0).
 * @param reversePlus    Reverse range extension (0â€“halfSpan, typically 0).
 * @param[out] outMax             Full-forward limit  (0â€“65535).
 * @param[out] outMin             Full-reverse limit  (0â€“65535).
 * @param[out] outMaxNeutral      Positive edge of takeoff dead-band (0â€“65535).
 * @param[out] outMinNeutral      Negative edge of takeoff dead-band (0â€“65535).
 */
void esc_calibrate(uint16_t halfSpan,
                   uint16_t deadBandHalf,
                   uint16_t reversePlus,
                   uint16_t* outMax,
                   uint16_t* outMin,
                   uint16_t* outMaxNeutral,
                   uint16_t* outMinNeutral);


// =============================================================================
// 2. INIT  (no-op when ESC_OUTPUT_ENABLED is absent)
// =============================================================================

/**
 * @brief Initialise all ESC hardware output channels.
 *
 * @details Iterates `devs[0..count-1]`, skipping clones (`parentID` set) and
 *   entries whose `signal` is neither `PWM_TWO_WAY_NEUTRAL_CENTER` nor `SERVO_SIG_NEUTRAL_CENTER`.
 *   For each active entry, claims pins via `reg` and attaches the appropriate driver object:
 *   - `DcDrvSignal::SERVO_SIG_NEUTRAL_CENTER` → `ServoCore` at `EscPwmFreq`, neutral at 1500 µs.
 *   - `DcDrvSignal::PWM_TWO_WAY_NEUTRAL_CENTER` → `DcMotorCore` at `EscPwmFreq`, optional
 *                                                `drvPort->dirPin` for bidirectional SpeedDir mode.
 *   Entries whose pin was not granted by `reg` are silently skipped.
 *
 * @param devs   Machine DC device array (must outlive all calls).
 * @param count  Number of entries in `devs`.
 * @param reg    Pin registry â€” used to resolve pin ownership.
 */
void esc_init(const DcDevice* devs, uint8_t count, PinReg& reg);


// =============================================================================
// 3. WRITE  (no-op when ESC_OUTPUT_ENABLED is absent)
// =============================================================================

/**
 * @brief Write a 16-bit ComBus command to one ESC channel.
 *
 * @details Maps the 16-bit ComBus value to the 1000â€“2000 Âµs hardware range
 *   internally, then dispatches to `ServoCore::writeMicroseconds()` for
 *   `SERVO_SIG_NEUTRAL_CENTER` or to `DcMotorCore::runAtSpeed()` for `PWM_TWO_WAY_NEUTRAL_CENTER`.
 *   Out-of-range `id` is silently ignored.
 *
 * @param id       Channel index matching the board's `EscCh` enum value.
 * @param cbusVal  Command in ComBus units (0 = full reverse, 32767 = neutral, 65535 = full forward).
 */
void esc_write(uint8_t id, uint16_t cbusVal);

// EOF esc.h
