/******************************************************************************
 * @file esc.h
 * @brief Board-agnostic ESC output dispatch — init, calibrate, write.
 *
 * @details Manages a runtime-sized array of ESC output channels without
 *   exposing hardware objects to the caller.  Configuration is supplied at
 *   init time through a `EscPort` array (board-specific, from the board
 *   header) so neither machine nor sound-module code needs direct access to
 *   `ServoCore` or `DcMotorCore`.
 *
 *   The module is usable by both the machine node and the sound node:
 *     - Machine node  : pass its own `escPortArray` in `esc_init()`.
 *     - Sound node    : same — `hw_init_esc.cpp` becomes a thin wrapper
 *                       delegating to this layer.
 *
 *   Activation: `-D ESC_OUTPUT_ENABLED` in build flags.
 *   All public functions compile to no-ops when the flag is absent.
 *
 *   Pulse range calibration is independent of hardware activation and is
 *   always available via `esc_calibrate()`.  This allows the inertia FSM
 *   (`esc_inertia`) to use correct range values even when no physical ESC
 *   is wired (e.g. sound-node standalone mode).
 *****************************************************************************/
#pragma once

#include <cstdint>
#include <struct/machines_struct.h>
#include "pin_reg.h"


// =============================================================================
// 1. TYPES
// =============================================================================

/**
 * @brief Dispatch hook for ESC output curve correction (QUICRUN, non-linear ESCs…).
 *
 * @details Used as an "aiguillage" (routing switch) in `map_esc_signal()`:
 *   - `nullptr`      → signal passes through unchanged (linear ESC, direct mapping).
 *   - non-`nullptr`  → the function is called with the raw inertial ComBus position
 *                      and must return the corrected value before the final
 *                      `[escMin..escMax] → [0..65535]` scaling is applied.
 *
 *   Typical use: `reMap(curveQuicrunFusion, val)` wrapper that indexes a
 *   pre-built correction table to compensate for non-linear motor response.
 *
 * @param val  Raw inertial position in ComBus units (0–65535).
 * @return     Corrected position in ComBus units (0–65535).
 */
using EscLinearizeFn = uint16_t (*)(uint16_t val);


// =============================================================================
// 2. PULSE RANGE CALIBRATION  (always available, independent of ESC_OUTPUT_ENABLED)
// =============================================================================

/**
 * @brief Compute the four ESC pulse-range calibration constants.
 *
 * @details Derives the hardware pulse range from three constexpr profile
 *   values.  The result is suitable for both the inertia FSM and for the
 *   signal mapping step that converts `escPulseWidthOut` → 1000–2000 µs.
 *
 *   All parameters and outputs are expressed in 16-bit ComBus units (0–65535).
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
 * @param halfSpan       Forward/reverse half-range (0–32767).
 *                       E.g. 32767 for a full-range ESC, 26214 for a 80%-range ESC.
 * @param deadBandHalf   Half-width of the takeoff dead-band (0–32767, typically 0).
 * @param reversePlus    Reverse range extension (0–halfSpan, typically 0).
 * @param[out] outMax             Full-forward limit  (0–65535).
 * @param[out] outMin             Full-reverse limit  (0–65535).
 * @param[out] outMaxNeutral      Positive edge of takeoff dead-band (0–65535).
 * @param[out] outMinNeutral      Negative edge of takeoff dead-band (0–65535).
 */
void esc_calibrate(uint16_t halfSpan,
                   uint16_t deadBandHalf,
                   uint16_t reversePlus,
                   uint16_t* outMax,
                   uint16_t* outMin,
                   uint16_t* outMaxNeutral,
                   uint16_t* outMinNeutral);


// =============================================================================
// 3. INIT  (no-op when ESC_OUTPUT_ENABLED is absent)
// =============================================================================

/**
 * @brief Initialise all ESC hardware output channels.
 *
 * @details Iterates `ports[0..count-1]`, claims pins via `reg`, and attaches
 *   the appropriate driver object:
 *   - `EscType::PWM_SERVO_SIG` → `ServoCore` at `EscPwmFreq`, neutral at 1500 µs
 *                                 (hardware init only — runtime commands go through `esc_write()`).
 *   - `EscType::PWM_HBRIDGE`   → `DcMotorCore` at `EscPwmFreq`, with optional dirPin
 *                                 for bidirectional SpeedDir mode.  Without dirPin,
 *                                 `runAtSpeed()` clamps negatives to 0 (unsigned PWM).
 *   Ports whose pin was not granted by `reg` are silently skipped.
 *
 * @param ports  Board ESC port configuration array (must outlive all calls).
 * @param count  Number of entries in `ports` (= `ESC_PORT_COUNT`).
 * @param reg    Pin registry — used to resolve pin ownership.
 */
void esc_init(const EscPort* ports, uint8_t count, PinReg& reg);


// =============================================================================
// 4. WRITE  (no-op when ESC_OUTPUT_ENABLED is absent)
// =============================================================================

/**
 * @brief Write a 16-bit ComBus command to one ESC channel.
 *
 * @details Maps the 16-bit ComBus value to the 1000–2000 µs hardware range
 *   internally, then dispatches to `ServoCore::writeMicroseconds()` for
 *   PWM_SERVO_SIG or to `DcMotorCore::runAtSpeed()` for PWM_HBRIDGE.
 *   Out-of-range `id` is silently ignored.
 *
 * @param id       Channel index matching the board's `EscCh` enum value.
 * @param cbusVal  Command in ComBus units (0 = full reverse, 32767 = neutral, 65535 = full forward).
 */
void esc_write(uint8_t id, uint16_t cbusVal);

// EOF esc.h
