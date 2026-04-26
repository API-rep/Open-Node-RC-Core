/******************************************************************************
 * @file light.cpp
 * @brief PWM-driven light and shaker channel init — board config driven.
 *****************************************************************************/

#include "light.h"
#include <core/system/hw/pin_reg.h>
#include <core/system/debug/debug.h>
#include <PwmBroker.h>
#include <memory>


// =============================================================================
// 1. MODULE STATE
// =============================================================================

static const LightPort*            s_light    = nullptr;
static statusLED*                  s_leds     = nullptr;     ///< Allocated by light_init(), indexed by LedCh.
static std::unique_ptr<PwmControl> s_controls[16];  ///< Indexed by LightPort slot (max 16 LEDC channels).


// =============================================================================
// 2. API
// =============================================================================

#if __has_include(<statusLED.h>)

/**
 * @brief Initialise all PWM-driven light channels.
 *
 *   Allocates a `statusLED[port->count]` array (`s_leds`) via placement new —
 *   one `statusLED(inversePolarity)` per channel using the board config — then
 *   iterates `port->cfg[]`, requests a PwmControl lease from PwmBroker for each
 *   active channel, and calls `statusLED::begin()`.
 *   Leases are stored in `s_controls[]` (indexed by slot) and outlive all calls.
 *
 *   Channels are skipped when:
 *     - pin < 0  (feature disabled, e.g. SOUND_NO_LED_PIN)
 *     - pin not owned by PinOwner::Light (claimed earlier by UART etc.)
 *     - PwmBroker::requestResource() returns nullptr (channel exhausted)
 *
 * @param port  Board LightPort — config array + channel count (must outlive all calls).
 * @param reg   Pin registry — used to skip pins claimed by higher-priority peripherals.
 */

void light_init(LightPort* port, PinReg& reg)
{
    if (LightPwmFreq < 1000u || LightPwmFreq > 40000u) {
        sys_log_err("[hw] LightPwmFreq %u Hz out of LEDC range [1000-40000] — light init aborted\n",
                    static_cast<unsigned>(LightPwmFreq));
        return;
    }

    s_light = port;
    if (!port || !port->cfg) return;

    // Allocate raw memory and placement-new each element with its own inversePolarity.
    // statusLED has no default constructor — per-channel polarity comes from board config.
    s_leds = static_cast<statusLED*>(::operator new[](sizeof(statusLED) * port->count));
    for (uint8_t i = 0; i < port->count; i++)
        new (&s_leds[i]) statusLED(port->cfg[i].inversePolarity);

    // Claim pins in registry — called after sys_init() has already claimed UART
    // and other high-priority pins, so shared-GPIO conflicts resolve in their favour.
    for (uint8_t i = 0; i < port->count; i++) {
        const int8_t pin = port->cfg[i].pin;
        if (pin < 0) continue;
        pin_claim(reg, static_cast<uint8_t>(pin), PinOwner::Light, port->cfg[i].infoName, false);
    }

    for (uint8_t i = 0; i < port->count && i < 16u; i++) {
        const int8_t pin = port->cfg[i].pin;
        if (pin < 0) continue;
        if (pin_owner_of(reg, static_cast<uint8_t>(pin)) != PinOwner::Light) {
            sys_log_warn("[hw] light ch%u '%s' GPIO%d: disabled — pin already claimed\n",
                         i, port->cfg[i].infoName, pin);
            continue;
        }

        s_controls[i] = PwmBroker::getInstance().requestResource(
                            static_cast<uint8_t>(pin), LightPwmFreq);
        if (!s_controls[i]) {
            sys_log_warn("[hw] light ch%u GPIO%d: PwmBroker request failed\n", i, pin);
            continue;
        }
        s_leds[i].begin(s_controls[i].get());
    }
}

/**
 * @brief Return the internally allocated statusLED array.
 *
 * @return Pointer to the `statusLED[port->count]` array allocated by
 *   `light_init()`, or `nullptr` before initialisation.
 *   Indexed by the vehicle LedCh enum (matches LightPort slot order).
 */
statusLED* light_leds_array()
{
    return s_leds;
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
