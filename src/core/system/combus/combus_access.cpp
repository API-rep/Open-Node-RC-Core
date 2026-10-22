/******************************************************************************
 * @file combus_access.cpp
 * @brief Layer-checked write accessors for ComBus channels.
 *
 * @details Write is skipped when the caller's ChanLayer does not have sufficient
 *   privileges for the channel's declared layer. Every denied write is counted
 *   (combus_denied_writes()) and reported once per channel through sys_log_warn()
 *   (DEBUG_SYSTEM builds), so a wrong caller layer cannot stay silent.
 *****************************************************************************/

#include "combus_access.h"

#include <core/system/debug/logging/debug.h>


// =============================================================================
// 1. INTERNAL HELPERS
// =============================================================================

/**
 * @brief Returns true when @p caller is authorised to write a slot with the given @p layer.
 *
 * @details Rules (in priority order):
 *   1. UNDEFINED → write denied for all callers (safety)
 *   2. BOARD     → only local firmware caller may write
 *   3. SYSTEM    → local firmware caller + extension boards caller of same node
 *   4. BASE      → any source (local + extensions + remote nodes caller)
 */
static inline bool _layer_ok(ChanLayer slot_layer, ChanLayer caller_layer) {
    const uint8_t raw_slot = static_cast<uint8_t>(slot_layer);
    const uint8_t raw_caller = static_cast<uint8_t>(caller_layer);
    
      // UNDEFINED → deny all writes (safety)
    if (raw_slot == static_cast<uint8_t>(ChanLayer::UNDEFINED)) {
        return false;
    }
    
      // BASE → allow all writes
    if (slot_layer == ChanLayer::BASE) {
        return true;
    }
    
      // SYSTEM → allow SYSTEM and BOARD callers (local firmware + extensions)
    if (slot_layer == ChanLayer::SYSTEM) {
        return (caller_layer == ChanLayer::SYSTEM) || (caller_layer == ChanLayer::BOARD);
    }
    
      // BOARD → only BOARD callers (local firmware only)
    if (slot_layer == ChanLayer::BOARD) {
        return caller_layer == ChanLayer::BOARD;
    }
    
      // Should never reach here
    return false;
}

    // Denied writes since boot, and one "already reported" bit per channel index
    // and per type (a channel index is a uint8_t).
static uint32_t s_deniedCount = 0u;
static uint8_t  s_reportedAnalog[32]  = {};
static uint8_t  s_reportedDigital[32] = {};


/**
 * @brief Count a denied write and report it once per channel.
 *
 * @details The count is always kept. The log line is printed the first time a
 *   given channel is denied, whatever the build flags allow sys_log_warn() to
 *   print, so a wrong caller layer shows up without flooding the log.
 */

static void _note_denied(const char* label, bool isAnalog, uint8_t ch,
                         ChanLayer caller, ChanLayer slot_layer) {
    ++s_deniedCount;

    uint8_t&      reported = (isAnalog ? s_reportedAnalog : s_reportedDigital)[ch >> 3];
    const uint8_t bit      = static_cast<uint8_t>(1u << (ch & 7u));
    if (reported & bit) { return; }
    reported |= bit;

    sys_log_warn("[COMBUS] write denied (reported once): %s ch=%u caller=%u layer=%u\n",
                 label,
                 static_cast<unsigned>(ch),
                 static_cast<unsigned>(caller),
                 static_cast<unsigned>(slot_layer));
}


// =============================================================================
// 2. CHANNEL WRITE ACCESSORS
// =============================================================================

bool combus_set_analog(ComBus& bus, AnalogComBusID ch, uint16_t val, ChanLayer caller) {
    auto& slot = bus.analogBus[static_cast<uint8_t>(ch)];

    if (!_layer_ok(slot.layer, caller)) {
          // --- Write denied ---
        _note_denied("analog", true, static_cast<uint8_t>(ch), caller, slot.layer);
        return false;
    }

    slot.value = val;
    return true;
}


bool combus_set_digital(ComBus& bus, DigitalComBusID ch, bool val, ChanLayer caller) {
    auto& slot = bus.digitalBus[static_cast<uint8_t>(ch)];

    if (!_layer_ok(slot.layer, caller)) {
          // --- Write denied ---
        _note_denied("digital", false, static_cast<uint8_t>(ch), caller, slot.layer);
        return false;
    }

    if (slot.bothOr) {
          // --- both_or: keep this board's value apart, value = local OR received ---
        slot.localValue   = val;
        slot.localWritten = true;
        slot.value        = val || slot.remoteValue;
        return true;
    }

    slot.value = val;
    return true;
}


void combus_both_or_set_remote(ComBus& bus, DigitalComBusID ch, bool remote) {
    auto& slot = bus.digitalBus[static_cast<uint8_t>(ch)];

    slot.remoteValue = remote;
    slot.value       = combus_both_or_local(slot) || remote;
}


uint32_t combus_denied_writes() {
    return s_deniedCount;
}


// EOF combus_access.cpp
