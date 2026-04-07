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

static const DcDevice*               s_ports  = nullptr;
static uint8_t                   s_count  = 0;
static std::unique_ptr<ServoCore[]>   s_srv;
static std::unique_ptr<DcMotorCore[]> s_dc;

#endif // ESC_OUTPUT_ENABLED


// =============================================================================
// 2. PULSE RANGE CALIBRATION
// =============================================================================

void esc_calibrate(uint16_t halfSpan,
                   uint16_t deadBandHalf,
                   uint16_t reversePlus,
                   uint16_t* outMax,
                   uint16_t* outMin,
                   uint16_t* outMaxNeutral,
                   uint16_t* outMinNeutral)
{
    if (!outMax || !outMin || !outMaxNeutral || !outMinNeutral) return;

    // All arithmetic in 32-bit to avoid silent wrapping on saturating inputs.
    static constexpr int32_t kNeutral = 32767;
    auto sat16 = [](int32_t v) -> uint16_t {
        return static_cast<uint16_t>(v < 0 ? 0 : v > 65535 ? 65535 : v);
    };

    *outMax        = sat16(kNeutral + halfSpan);
    *outMin        = sat16(kNeutral - halfSpan + reversePlus);
    *outMaxNeutral = sat16(kNeutral + deadBandHalf);
    *outMinNeutral = sat16(kNeutral - deadBandHalf);
}


// =============================================================================
// 3. INIT
// =============================================================================

#ifdef ESC_OUTPUT_ENABLED

/**
 * @brief Pre-allocate driver arrays and attach each port.
 *
 * @details Allocates `count` ServoCore and DcMotorCore instances.  Only the
 *   slots matching the active mode are actually attached — the other
 *   slots remain default-constructed (no PWM channel consumed).
 *   Silently skips any port whose pin is not owned by PinOwner::Esc in `reg`.
 */
void esc_init(const DcDevice* devs, uint8_t count, PinReg& reg)
{
    if (!devs || count == 0) return;

    // --- 1. Store pointer and allocate driver arrays ---
    s_ports = devs;
    s_count = count;
    s_srv = std::make_unique<ServoCore[]>(count);
    s_dc  = std::make_unique<DcMotorCore[]>(count);

    // --- 2. Iterate devices and attach each ---
    for (uint8_t i = 0; i < count; i++) {
        if (devs[i].parentID.has_value())               continue; // clone — shares parent hardware
        if (devs[i].signal != DcDrvSignal::PWM_TWO_WAY_NEUTRAL_CENTER &&
            devs[i].signal != DcDrvSignal::SERVO_SIG_NEUTRAL_CENTER) continue; // not an ESC device
        if (!devs[i].drvPort)                           continue; // not wired
        if (!devs[i].drvPort->pwmPin.has_value())       continue;

        if (devs[i].signal == DcDrvSignal::SERVO_SIG_NEUTRAL_CENTER) {
            // --- 2.1 Servo-signal ESC ---
            if (pin_resolve(reg, devs[i].drvPort->pwmPin.value(), PinOwner::Esc) == PIN_SLOT_EMPTY) continue;
            s_srv[i].setPwmFreq(EscPwmFreq);
            s_srv[i].begin(static_cast<int8_t>(devs[i].drvPort->pwmPin.value()));
            s_srv[i].writeMicroseconds(1500);
            sys_log_dbg("[esc] ch%u: SERVO_SIG on GPIO%u\n", i, devs[i].drvPort->pwmPin.value());

        } else if (devs[i].signal == DcDrvSignal::PWM_TWO_WAY_NEUTRAL_CENTER) {
            // --- 2.2 H-bridge DC motor driver ---
            if (pin_resolve(reg, devs[i].drvPort->pwmPin.value(), PinOwner::Esc) == PIN_SLOT_EMPTY) continue;
            s_dc[i].setPwmFreq(EscPwmFreq);
            std::optional<int8_t> dir = std::nullopt;
            if (devs[i].drvPort->dirPin.has_value())
                dir = static_cast<int8_t>(devs[i].drvPort->dirPin.value());
            s_dc[i].attach(static_cast<int8_t>(devs[i].drvPort->pwmPin.value()), dir);
            s_dc[i].enable();
            sys_log_dbg("[esc] ch%u: PWM_BIPOLAR on GPIO%u\n", i, devs[i].drvPort->pwmPin.value());
        }
    }
}

#else

void esc_init(const DcDevice* /*devs*/, uint8_t /*count*/, PinReg& /*reg*/) {}

#endif // ESC_OUTPUT_ENABLED


// =============================================================================
// 4. WRITE
// =============================================================================

#ifdef ESC_OUTPUT_ENABLED

void esc_write(uint8_t id, uint16_t cbusVal)
{
    if (id >= s_count || !s_ports) return;

    if (s_ports[id].signal == DcDrvSignal::SERVO_SIG_NEUTRAL_CENTER) {
        // 16-bit → 1000–2000 µs servo pulse
        const uint16_t us = static_cast<uint16_t>(
            1000u + static_cast<uint32_t>(cbusVal) * 1000u / 65535u);
        s_srv[id].writeMicroseconds(us);

    } else if (s_ports[id].signal == DcDrvSignal::PWM_TWO_WAY_NEUTRAL_CENTER) {
        // 16-bit → -100.0 … +100.0 %   (neutral = 32767, not 32768 — unavoidable 1-lsb asymmetry)
        // DcMotorCore::runAtSpeed() range is [-100, +100] regardless of dirPin presence:
        //   with dirPin    → SpeedDir mode  (bidirectional, lib clamps |speed| to [0, 100])
        //   without dirPin → unsigned PWM   (negatives clamped to 0 inside lib)
        const float speed = (static_cast<float>(cbusVal) - 32767.0f) * (100.0f / 32767.0f);
        s_dc[id].runAtSpeed(speed);
    }
}

#else

void esc_write(uint8_t /*id*/, uint16_t /*signalUs*/) {}

#endif // ESC_OUTPUT_ENABLED

// EOF esc.cpp
