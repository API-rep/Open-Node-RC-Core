/******************************************************************************
 * @file light.cpp
 * @brief PWM-driven light and shaker channel init — board config driven.
 *****************************************************************************/

#include "light.h"
#include "pin_reg.h"
#include <core/system/debug/debug.h>
#include <PwmBroker.h>
#include <memory>


// =============================================================================
// 1. MODULE STATE
// =============================================================================

static const LightPort*            s_light = nullptr;
static std::unique_ptr<PwmControl> s_controls[16];  ///< Indexed by LightPort slot (max 16 LEDC channels).


// =============================================================================
// 2. API
// =============================================================================

#if __has_include(<statusLED.h>)

/**
 * @brief Initialise all PWM-driven light channels.
 *
 * @details Iterates `port->cfg[]`, requests a PwmControl lease from PwmBroker
 *   for each active channel, and calls `statusLED::begin()` with the raw pointer.
 *   Leases are stored in `s_controls[]` (indexed by slot) and outlive all calls.
 *
 *   Channels are skipped when:
 *     - pin < 0  (feature disabled, e.g. SOUND_NO_LED_PIN)
 *     - pin not owned by PinOwner::Light (claimed earlier by UART etc.)
 *     - PwmBroker::requestResource() returns nullptr (channel exhausted)
 *
 * @param port  Board LightPort — config array + channel count (must outlive all calls).
 * @param leds  Caller-owned `statusLED*` array, same order as the Light enum.
 * @param reg   Pin registry — used to skip pins claimed by higher-priority peripherals.
 */

void light_init(LightPort* port, statusLED** leds, PinReg& reg)
{
    if (LightPwmFreq < 1000u || LightPwmFreq > 40000u) {
        sys_log_err("[hw] LightPwmFreq %u Hz out of LEDC range [1000-40000] — light init aborted\n",
                    static_cast<unsigned>(LightPwmFreq));
        return;
    }

    s_light = port;
    if (!port || !port->cfg || !leds) return;

    for (uint8_t i = 0; i < port->count && i < 16u; i++) {
        const int8_t pin = port->cfg[i].pin;
        if (pin < 0) continue;
        if (pin_owner_of(reg, static_cast<uint8_t>(pin)) != PinOwner::Light) continue;

        s_controls[i] = PwmBroker::getInstance().requestResource(
                            static_cast<uint8_t>(pin), LightPwmFreq);
        if (!s_controls[i]) {
            sys_log_warn("[hw] light ch%u GPIO%d: PwmBroker request failed\n", i, pin);
            continue;
        }
        leds[i]->begin(s_controls[i].get());
    }
}

#endif  // __has_include(<statusLED.h>)


/**
 * @brief Return the number of registered light channels.
 *
 * @return Channel count from the active `LightPort`, or 0 if not initialised.
 */
uint8_t light_count()
{
    // 0 before light_init().
    return s_light ? s_light->count : 0;
}

// EOF light.cpp
