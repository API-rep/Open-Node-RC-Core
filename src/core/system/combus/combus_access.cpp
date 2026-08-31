/******************************************************************************
 * @file combus_access.cpp
 * @brief Layer-checked write accessors for ComBus channels.
 *
 * @details Write is skipped when the caller's ChanLayer does not have sufficient
 *   privileges for the channel's declared layer. In DEBUG_COMBUS builds a warning
 *   line is printed on every denied write.
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
 *   2. SYSTEM    → only local firmware caller may write
 *   3. LOCAL     → local firmware caller + extension boards caller of same node
 *   4. REMOTE    → any source (local + extensions + remote nodes caller)
 */
static inline bool _layer_ok(ChanLayer slot_layer, ChanLayer caller_layer) {
    const uint8_t raw_slot = static_cast<uint8_t>(slot_layer);
    const uint8_t raw_caller = static_cast<uint8_t>(caller_layer);
    
      // UNDEFINED → deny all writes (safety)
    if (raw_slot == static_cast<uint8_t>(ChanLayer::UNDEFINED)) {
        return false;
    }
    
      // REMOTE → allow all writes
    if (slot_layer == ChanLayer::REMOTE) {
        return true;
    }
    
      // LOCAL → allow LOCAL and SYSTEM callers (local firmware + extensions)
    if (slot_layer == ChanLayer::LOCAL) {
        return (caller_layer == ChanLayer::LOCAL) || (caller_layer == ChanLayer::SYSTEM);
    }
    
      // SYSTEM → only SYSTEM callers (local firmware only)
    if (slot_layer == ChanLayer::SYSTEM) {
        return caller_layer == ChanLayer::SYSTEM;
    }
    
      // Should never reach here
    return false;
}

#ifdef DEBUG_COMBUS

static void _warn_denied(const char* label, uint8_t ch,
                          ChanLayer caller, ChanLayer slot_layer) {
    sys_log_warn("[COMBUS] write denied: %s ch=%u caller=%u layer=%u\n",
                 label,
                 static_cast<unsigned>(ch),
                 static_cast<unsigned>(caller),
                 static_cast<unsigned>(slot_layer));
}

#else
  // No-op in release builds — layer check still runs, logs are suppressed.
  #define _warn_denied(label, ch, caller, slot_layer)  ((void)0)
#endif


// =============================================================================
// 2. CHANNEL WRITE ACCESSORS
// =============================================================================

bool combus_set_analog(ComBus& bus, AnalogComBusID ch, uint16_t val, ChanLayer caller) {
    auto& slot = bus.analogBus[static_cast<uint8_t>(ch)];

    if (!_layer_ok(slot.layer, caller)) {
          // --- Write denied ---
        _warn_denied("analog", static_cast<uint8_t>(ch), caller, slot.layer);
        return false;
    }

    slot.value = val;
    return true;
}


bool combus_set_digital(ComBus& bus, DigitalComBusID ch, bool val, ChanLayer caller) {
    auto& slot = bus.digitalBus[static_cast<uint8_t>(ch)];

    if (!_layer_ok(slot.layer, caller)) {
          // --- Write denied ---
        _warn_denied("digital", static_cast<uint8_t>(ch), caller, slot.layer);
        return false;
    }

    slot.value = val;
    return true;
}


// =============================================================================
// 3. HEADER FIELD WRITE ACCESSORS
// =============================================================================

bool combus_set_runlevel(ComBus& bus, RunLevel rl, ChanLayer caller) {
    if (!_layer_ok(bus.runLevelLayer, caller)) {
        _warn_denied("runLevel", 0xFF, caller, bus.runLevelLayer);
        return false;
    }
    bus.runLevel = rl;
    return true;
}


// EOF combus_access.cpp
