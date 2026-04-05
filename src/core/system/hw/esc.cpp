/******************************************************************************
 * @file esc.cpp
 * @brief Board-agnostic ESC output dispatch — implementation.
 *
 * @details Owns runtime-allocated `ServoCore` and `DcMotorCore` arrays indexed
 *   by the board's `EscCh` enum values.  The caller never accesses these
 *   objects directly — all interaction goes through `esc_init()` and
 *   `esc_write()`.  The port table pointer is stored at init and used at
 *   every `esc_write()` call for protocol dispatch.
 *
 *   `esc_calibrate()` has no hardware dependency and is always compiled.
 *****************************************************************************/

#include "esc.h"
#include <core/system/debug/debug.h>

#ifdef ESC_OUTPUT_ENABLED
#  include <ServoCore.h>
#  include <DcMotorCore.h>
#  include <memory>
#endif


// =============================================================================
// 1. MODULE STATE
// =============================================================================

#ifdef ESC_OUTPUT_ENABLED

static const EscPort*            s_ports  = nullptr;
static uint8_t                   s_count  = 0;
static std::unique_ptr<ServoCore[]>   s_srv;
static std::unique_ptr<DcMotorCore[]> s_dc;

#endif // ESC_OUTPUT_ENABLED


// =============================================================================
// 2. PULSE RANGE CALIBRATION
// =============================================================================

void esc_calibrate(uint16_t span,
                   uint16_t takeoffPunch,
                   uint16_t reversePlus,
                   uint16_t* outMax,
                   uint16_t* outMin,
                   uint16_t* outMaxNeutral,
                   uint16_t* outMinNeutral)
{
    if (!outMax || !outMin || !outMaxNeutral || !outMinNeutral) return;

    *outMaxNeutral = static_cast<uint16_t>(1500u + takeoffPunch);
    *outMinNeutral = static_cast<uint16_t>(1500u - takeoffPunch);
    *outMax        = static_cast<uint16_t>(1500u + span);
    *outMin        = static_cast<uint16_t>(1500u - span + reversePlus);
}


// =============================================================================
// 3. INIT
// =============================================================================

#ifdef ESC_OUTPUT_ENABLED

/**
 * @brief Pre-allocate driver arrays and attach each port.
 *
 * @details Allocates `count` ServoCore and DcMotorCore instances.  Only the
 *   slots matching the active EscType are actually attached — the other
 *   slots remain default-constructed (no PWM channel consumed).
 *   Silently skips any port whose pin is not owned by PinOwner::Esc in `reg`.
 */
void esc_init(const EscPort* ports, uint8_t count, PinReg& reg)
{
    if (!ports || count == 0) return;

    // --- 1. Allocate both arrays — only matching-type slots are attached ---
    s_ports = ports;
    s_count = count;
    s_srv = std::make_unique<ServoCore[]>(count);
    s_dc  = std::make_unique<DcMotorCore[]>(count);

    // --- 2. Iterate ports and attach each ---
    for (uint8_t i = 0; i < count; i++) {
        if (!ports[i].pwmPin.has_value()) continue;

        if (ports[i].escType == EscType::PWM_SERVO_SIG) {
            // --- 2.1 Servo-signal ESC ---
            if (pin_resolve(reg, ports[i].pwmPin.value(), PinOwner::Esc) == PIN_SLOT_EMPTY) continue;
            s_srv[i].setPwmFreq(EscPwmFreq);
            s_srv[i].begin(static_cast<int8_t>(ports[i].pwmPin.value()));
            s_srv[i].writeMicroseconds(1500);
            sys_log_dbg("[esc] ch%u: PWM_SERVO_SIG on GPIO%u\n", i, ports[i].pwmPin.value());

        } else if (ports[i].escType == EscType::PWM_HBRIDGE) {
            // --- 2.2 H-bridge DC motor driver ---
            if (pin_resolve(reg, ports[i].pwmPin.value(), PinOwner::Esc) == PIN_SLOT_EMPTY) continue;
            s_dc[i].setPwmFreq(EscPwmFreq);
            std::optional<int8_t> dir = std::nullopt;
            if (ports[i].dirPin.has_value())
                dir = static_cast<int8_t>(ports[i].dirPin.value());
            s_dc[i].attach(static_cast<int8_t>(ports[i].pwmPin.value()), dir);
            s_dc[i].enable();
            sys_log_dbg("[esc] ch%u: PWM_HBRIDGE on GPIO%u\n", i, ports[i].pwmPin.value());
        }
    }
}

#else

void esc_init(const EscPort* /*ports*/, uint8_t /*count*/, PinReg& /*reg*/) {}

#endif // ESC_OUTPUT_ENABLED


// =============================================================================
// 4. WRITE
// =============================================================================

#ifdef ESC_OUTPUT_ENABLED

void esc_write(uint8_t id, uint16_t signalUs)
{
    if (id >= s_count || !s_ports) return;

    if (s_ports[id].escType == EscType::PWM_SERVO_SIG) {
        s_srv[id].writeMicroseconds(signalUs);

    } else if (s_ports[id].escType == EscType::PWM_HBRIDGE) {
        float speed = (static_cast<float>(signalUs) - 1500.0f) / 5.0f;
        s_dc[id].runAtSpeed(speed);
    }
}

#else

void esc_write(uint8_t /*id*/, uint16_t /*signalUs*/) {}

#endif // ESC_OUTPUT_ENABLED

// EOF esc.cpp
