/*!****************************************************************************
 * @file  pin_reg.cpp
 * @brief GPIO pin registry implementation.
 *
 * @details The registry is a flat `PinRegEntry[]` array (embedded in `PinReg`)
 *   scanned linearly on each claim or query.
 *
 *   Claim operations run once during the init sequence (never in the main
 *   loop),so (inefficient) linear scan is acceptable for the small arrays
 *   (typically ≤ 32 pins per environment).
 *
 *   Conflict policy:
 *     - Critical pin conflict  → `sys_log_err` + infinite loop (fatal halt).
 *     - Registry-full overflow → always fatal (misconfigured `PinRegSize`).
 *     - Non-critical conflict  → `sys_log_warn` + `false` return; caller
 *       must skip the peripheral's `begin()` call.
 *****************************************************************************/

#include "pin_reg.h"

#include <Arduino.h>
#include <core/system/debug/debug.h>


// =============================================================================
// 1. INTERNAL HELPERS
// =============================================================================

/** @brief Return a pointer to the entry for `pin`, or nullptr if not found. */

static PinRegEntry* find_slot(PinReg& reg, uint8_t pin) {
    for (uint8_t i = 0; i < reg.pinEntryCursor; i++) {
        if (reg.pinRegEntry[i].pin == pin) return &reg.pinRegEntry[i];
    }
    return nullptr;
}


/** @brief Human-readable name for a PinOwner value (for log output). */

static const char* owner_name(PinOwner o) {
    switch (o) {
        case PinOwner::Uart0:      return "Uart0";
        case PinOwner::Uart1:      return "Uart1";
        case PinOwner::Uart2:      return "Uart2";
        case PinOwner::DcDrv:      return "DcDrv";
        case PinOwner::DcDrvPwm:   return "DcDrvPwm";
        case PinOwner::DcDrvDir:   return "DcDrvDir";
        case PinOwner::DcDrvBrk:   return "DcDrvBrk";
        case PinOwner::DcDrvEn:    return "DcDrvEn";
        case PinOwner::DcDrvSlp:   return "DcDrvSlp";
        case PinOwner::DcDrvFlt:   return "DcDrvFlt";
        case PinOwner::Light:      return "Light";
        case PinOwner::NeoPixel:   return "NeoPixel";
        case PinOwner::ServoOut:   return "ServoOut";
        case PinOwner::Esc:        return "Esc";
        case PinOwner::Shaker:     return "Shaker";
        case PinOwner::Sound:      return "Sound";
        case PinOwner::Vbat:       return "Vbat";
        case PinOwner::ServoIn:    return "ServoIn";
        case PinOwner::Switch:     return "Switch";
        default:                   return "Free";
    }
}


// =============================================================================
// 2. LIFECYCLE
// =============================================================================

/**
 * @brief Allocate and zero-initialise the pin registry.
 *
 * @details Allocates a `PinRegEntry[size]` array via `new` and stores
 *   it inside `reg`. Every slot is set to `{PIN_SLOT_EMPTY, Free, nullptr}`.
 *   The cursor is reset to 0 — any previous content is discarded.
 *
 *   Must be called exactly once per environment, before any `pin_claim()`.
 *   `size` should match the board-level `PinRegMaxEntry` constant.
 *
 * @param reg   Registry to initialise (output).
 * @param size  Number of slots to allocate (= max claimable pins).
 */

void pin_reg_init(PinReg& reg, uint8_t size) {
    reg.pinRegEntry      = new PinRegEntry[size];
    reg.pinRegEntryCount = size;
    reg.pinEntryCursor   = 0;
    for (uint8_t i = 0; i < size; i++) {
        reg.pinRegEntry[i] = { PIN_SLOT_EMPTY, PinOwner::Free, nullptr, false };
    }
}


// =============================================================================
// 3. CLAIMING
// =============================================================================

/**
 * @brief Attempt to claim one GPIO pin.
 *
 * @details Sequence:
 *   1. Linear scan for an existing claim on `pin`.
 *   2. On conflict: fatal halt (critical) or warn + return false (non-critical).
 *   3. Guard: registry full → always fatal (PinRegSize too small).
 *   4. Write new entry, increment count.
 */
bool pin_claim(PinReg& reg, uint8_t pin, PinOwner owner, const char* label, bool critical, bool sharedPin) {

        // 1. Existing claim check
    PinRegEntry* existing = find_slot(reg, pin);
    if (existing != nullptr) {
        if (existing->owner == owner &&
            existing->sharedPin &&
            sharedPin) {
            return true;
        }

        if (critical) {
            sys_log_err("[pin_reg] FATAL CONFLICT GPIO%u: '%s' (%s) already claimed — '%s' (%s) rejected\n",
                        pin,
                        existing->label,
                        owner_name(existing->owner),
                        label,
                        owner_name(owner));
            while (true) {}
        }
        sys_log_warn("[pin_reg] conflict GPIO%u: '%s' (%s) already claimed — '%s' (%s) skipped\n",
                     pin,
                     existing->label,
                     owner_name(existing->owner),
                     label,
                     owner_name(owner));
        return false;
    }

        // 2. Registry-full guard (always fatal)
    if (reg.pinEntryCursor >= reg.pinRegEntryCount) {
        sys_log_err("[pin_reg] FATAL: registry full (max=%u) — GPIO%u '%s' not recorded\n",
                    reg.pinRegEntryCount, pin, label);
        while (true) {}
    }

        // 3. Record claim
    reg.pinRegEntry[reg.pinEntryCursor++] = { pin, owner, label, sharedPin };
    return true;
}


/**
 * @brief Claim a batch of pins from a board config array.
 *
 * @details Iterates `descs[0..count-1]` and calls `pin_claim()` for each
 *   entry, forwarding its `role`, `label` and `critical` flag.
 *
 * @param reg    Pin registry to populate.
 * @param descs  Board-defined `PinDesc[]` array.
 * @param count  Number of entries in `descs`.
 * @return Number of pins successfully claimed (conflicts excluded).
 */
uint8_t pin_claim_batch(PinReg& reg, const PinDesc* descs, uint8_t count) {
    uint8_t claimed = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (descs[i].pin == PIN_SLOT_EMPTY) {
            continue;   // skip disabled / no-pin entries
        }

        if (pin_claim(reg, descs[i].pin, descs[i].role, descs[i].label, descs[i].critical, descs[i].sharedPin)){
            claimed++;
        }
    }
    return claimed;
}


// =============================================================================
// 4. QUERY
// =============================================================================

/** @brief Return the owner of `pin`, or `PinOwner::Free` if unclaimed. */

PinOwner pin_owner_of(const PinReg& reg, uint8_t pin) {
    for (uint8_t i = 0; i < reg.pinEntryCursor; i++) {
        if (reg.pinRegEntry[i].pin == pin) return reg.pinRegEntry[i].owner;
    }
    return PinOwner::Free;
}

/**
 * @brief Return `pin` only if it is claimed by `owner`, else `PIN_SLOT_EMPTY`.
 *
 * @details Lets a module guard a pin access behind an ownership check in a
 *   single call — avoids the two-step `pin_owner_of` + compare pattern.
 */

uint8_t pin_resolve(const PinReg& reg, uint8_t pin, PinOwner owner) {
    for (uint8_t i = 0; i < reg.pinEntryCursor; i++) {
        if (reg.pinRegEntry[i].pin == pin && reg.pinRegEntry[i].owner == owner) return pin;
    }
    return PIN_SLOT_EMPTY;
}


// =============================================================================
// 5. DIAGNOSTICS
// =============================================================================

/**
 * @brief Dump the full registry to the debug log.
 *
 * @details Output is gated by DEBUG_SYSTEM (sys_log_info is a no-op when
 *   the flag is absent — zero overhead in release builds).
 */
void pin_reg_dump(const PinReg& reg) {
    sys_log_info("[pin_reg] GPIO registry — %u/%u claimed:\n", reg.pinEntryCursor, reg.pinRegEntryCount);
    for (uint8_t i = 0; i < reg.pinEntryCursor; i++) {
        const PinRegEntry& e = reg.pinRegEntry[i];

        sys_log_info("[pin_reg]   GPIO%-2u  %-10s  shared=%c  %s\n",
                     e.pin,
                     owner_name(e.owner),
                     e.sharedPin ? 'Y' : 'N',
                     e.label ? e.label : "");
    }
}

// EOF pin_reg.cpp
