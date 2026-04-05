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

/// Function pointer for ESC output linearization (QUICRUN and similar).
/// @param pw  Raw pulse-width value (1000–2000 µs).
/// @return    Linearized pulse-width value.
/// Pass `nullptr` to `EscInertiaConfig::linearizeFn` for a direct passthrough.
using EscLinearizeFn = uint16_t (*)(uint16_t pw);


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
 *   Formulas:
 *   @code
 *     escPulseMaxNeutral = 1500 + takeoffPunch
 *     escPulseMinNeutral = 1500 - takeoffPunch
 *     escPulseMax        = 1500 + span
 *     escPulseMin        = 1500 - span + reversePlus
 *   @endcode
 *
 * @param span           Half-range in µs (EscPulseSpan, e.g. 600).
 * @param takeoffPunch   Dead-band at neutral in µs (EscTakeoffPunch, e.g. 0).
 * @param reversePlus    Reverse range extension in µs (EscReversePlus, e.g. 0).
 * @param[out] outMax             Full-forward limit.
 * @param[out] outMin             Full-reverse limit.
 * @param[out] outMaxNeutral      Positive edge of takeoff dead-band.
 * @param[out] outMinNeutral      Negative edge of takeoff dead-band.
 */
void esc_calibrate(uint16_t span,
                   uint16_t takeoffPunch,
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
 *   - `EscType::PWM_SERVO_SIG` → `ServoCore` at `EscPwmFreq`, neutral 1500 µs.
 *   - `EscType::PWM_HBRIDGE`   → `DcMotorCore` at `EscPwmFreq`.
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
 * @brief Write a 1000–2000 µs command to one ESC channel.
 *
 * @details Dispatches to `ServoCore::writeMicroseconds()` for PWM_SERVO_SIG
 *   or to `DcMotorCore::runAtSpeed((us-1500)/5)` for PWM_HBRIDGE.
 *   Out-of-range `id` is silently ignored.
 *
 * @param id        Channel index matching the board's `EscCh` enum value.
 * @param signalUs  Command in µs (1000 = full reverse, 1500 = neutral, 2000 = full forward).
 */
void esc_write(uint8_t id, uint16_t signalUs);

// EOF esc.h
