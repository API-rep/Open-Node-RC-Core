/*!****************************************************************************
 * @file  pin_reg.h
 * @brief Centralised registry for GPIO pin ownership.
 *
 * @details This module provides a centralised registry to track GPIO pin 
 *  ownership and manage conflicts at init time. It can occur when a same
 *  pin is set in multiple submodule configuration arrays (shared pins).
 *
 *  At init time, when a GPIO is requested by submodules, a read of pinRegEntry[]
 *  slot is performed. If no existing claim is found, the new claim is recorded in
 *  the first free slot. If an existing claim is found, the conflict is resolved
 *  according to the `critical` flag of the claimant.
 *  - Critical claim: logs the conflict and halts the init.
 *  - Non-critical claim: logs the conflict and skips the peripheral's init.
 *
 *   Pin ownership is set by init order. High-priority peripherals (UART,
 *   ComBus) must claim before lower-priority ones (LED, NeoPixel, coupler sense).
 *
 *   Typical init sequence — GPIO27 shared between RGB LED and coupler sense:
 *   @code
 *       PinReg pinReg;
 *       pin_reg_init(pinReg, PinRegMaxEntry);
 *
 *       // 1. Critical peripherals first (fatal on conflict)
 *       //    kPinsSerial[] lists GPIO1, 3, 16 — always unique, critical=true
 *       pin_claim_batch(pinReg, kPinsSerial, kPinsSerialCount);
 *
 *       // 2. Non-critical: kPinsLed[] may list GPIO27 (NeoPixel)
 *       pin_claim_batch(pinReg, kPinsLed, kPinsLedCount);
 *
 *       // 3. Non-critical: kPinsSignal[] also lists GPIO27 (coupler sense)
 *       //    → conflict detected here, "COUPLER" claim is skipped, logged
 *       pin_claim_batch(pinReg, kPinsSignal, kPinsSignalCount);
 *
 *       // 4. Resolve at peripheral init — use whichever owner actually won
 *       if (pin_resolve(pinReg, LED_PIN, PinOwner::NeoPixel) != PIN_SLOT_EMPTY)
 *           FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
 *
 *       if (pin_resolve(pinReg, COUPLER_PIN, PinOwner::DigitalIn) != PIN_SLOT_EMPTY)
 *           pinMode(COUPLER_PIN, INPUT_PULLUP);
 *
 *       pin_reg_dump(pinReg);  // log full claim table (gated by DEBUG_SYSTEM)
 *   @endcode
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <struct/pin_struct.h>


// =============================================================================
// 1. LIFECYCLE
// =============================================================================

/**
 * @brief Initialise the pin registry.
 *
 * @details Must be called once before any `pin_claim*()` call.
 *   Allocates the backing array, records its capacity, zeroes the cursor,
 *   and fills all slots with `PIN_SLOT_EMPTY`.
 *
 * @param pinReg  Pin registry container to initialise.
 * @param size    Number of slots (= board's `PinRegMaxEntry`).
 */

void pin_reg_init(PinReg& pinReg, uint8_t size);


// =============================================================================
// 2. CLAIMING
// =============================================================================

/**
 * @brief Attempt to claim one GPIO pin for an owner.
 *
 * @details On conflict:
 *   - `critical = true`  → logs + halts firmware (`while(true)`).
 *   - `critical = false` → logs + returns `false`; caller skips the peripheral.
 *
 * @return `true` on success, `false` on non-critical conflict.
 */

bool pin_claim(PinReg& pinReg, uint8_t pin, PinOwner owner, const char* label, bool critical = false);


/**
 * @brief Claim a batch of pins  from a board theme descriptor array.
 *
 * @details Calls `pin_claim()` for each `PinDesc` entry using its `critical`
 *   flag.  Returns the number of pins successfully claimed.
 */

uint8_t pin_claim_batch(PinReg& pinReg, const PinDesc* descs, uint8_t count);



// =============================================================================
// 3. QUERY
// =============================================================================

/**
 * @brief Return the current owner of `pin`.
 *
 * @param pinReg  Pin registry to query.
 * @param pin     GPIO number to look up.
 * @return Owner category, or `PinOwner::Free` if unclaimed.
 */

PinOwner pin_owner_of(const PinReg& pinReg, uint8_t pin);


/**
 * @brief Return `pin` if it is owned by `owner`, else `PIN_SLOT_EMPTY`.
 *
 * @details Convenience helper for init code:
 *   @code
 *   uint8_t p = pin_resolve(pinReg, FOGLIGHT_GPIO, PinOwner::LedLedc);
 *   if (p != PIN_SLOT_EMPTY) fogLight.begin(p, 5, 20000);
 *   @endcode
 */
uint8_t pin_resolve(const PinReg& pinReg, uint8_t pin, PinOwner owner);


// =============================================================================
// 4. DIAGNOSTICS
// =============================================================================

/// Dump the full registry to the debug log (no-op when DEBUG_SYSTEM is absent).
void pin_reg_dump(const PinReg& pinReg);

// EOF pin_reg.h
