/******************************************************************************
 * @file combus_access.cpp
 * @brief Ownership-checked write accessors for ComBus channels.
 *
 * @details Write is skipped when the caller's ChanOwner does not match the
 *   channel's declared owner, unless the channel is NONE (unguarded) or ANY.
 *   In DEBUG_COMBUS builds a warning line is printed on every denied write.
 *****************************************************************************/

#include "combus_access.h"

#include <core/system/debug/debug.h>


// =============================================================================
// 1. NODE GROUP STATE  (set once at combus_init)
// =============================================================================

/** NodeGroup of the current build environment — set by combus_set_node_group(). */
static uint8_t s_envNodeGroup = ComBusOwner::GRP_NONE;

void combus_set_node_group(uint8_t group) {
    s_envNodeGroup = group;
}


// =============================================================================
// 2. INTERNAL HELPERS
// =============================================================================

/**
 * @brief Returns true when @p caller is authorised to write a slot with the given @p owner.
 *
 * @details Rules (in priority order):
 *   1. NONE (0x00)            — unclaimed; deny all.
 *   2. ANY  (0xFF)            — unrestricted; allow all.
 *   3. GRP_ANY, PROC != ANY   — wildcard group: any node's matching process.
 *   4. slot.group == my group — local domain: full byte match required.
 *   5. otherwise              — foreign domain: PROC_BRIDGE only.
 */
static inline bool _owner_ok(ChanOwner slot, ChanOwner caller) {
    using namespace ComBusOwner;
    const uint8_t raw_s = static_cast<uint8_t>(slot);
    const uint8_t raw_c = static_cast<uint8_t>(caller);
    const uint8_t s_grp = raw_s & GRP_MASK;
    const uint8_t s_prc = raw_s & PROC_MASK;
    const uint8_t c_prc = raw_c & PROC_MASK;

    if (raw_s == 0x00u)       return false;           // NONE — unclaimed, deny all
    if (raw_s == 0xFFu)       return true;            // ANY  — unrestricted, allow all
    if (s_grp == GRP_ANY)     return s_prc == c_prc;  // wildcard group — match process only
    if (s_grp == s_envNodeGroup)  return raw_s == raw_c;  // local domain — full byte match
    return c_prc == PROC_BRIDGE;                      // foreign domain — bridge only
}

#ifdef DEBUG_COMBUS

static void _warn_denied(const char* label, uint8_t ch,
                          ChanOwner caller, ChanOwner slot_owner) {
    sys_log_warn("[COMBUS] write denied: %s ch=%u caller=%u owner=%u\n",
                 label,
                 static_cast<unsigned>(ch),
                 static_cast<unsigned>(caller),
                 static_cast<unsigned>(slot_owner));
}

#else
  // No-op in release builds — ownership check still runs, logs are suppressed.
  #define _warn_denied(label, ch, caller, slot_owner)  ((void)0)
#endif


// =============================================================================
// 2. CHANNEL WRITE ACCESSORS
// =============================================================================

bool combus_set_analog(ComBus& bus, AnalogComBusID ch, uint16_t val, ChanOwner caller) {
    auto& slot = bus.analogBus[static_cast<uint8_t>(ch)];

    if (!_owner_ok(slot.owner, caller)) {
          // --- Write denied ---
        _warn_denied("analog", static_cast<uint8_t>(ch), caller, slot.owner);
        return false;
    }

    slot.value = val;
    return true;
}


bool combus_set_digital(ComBus& bus, DigitalComBusID ch, bool val, ChanOwner caller) {
    auto& slot = bus.digitalBus[static_cast<uint8_t>(ch)];

    if (!_owner_ok(slot.owner, caller)) {
          // --- Write denied ---
        _warn_denied("digital", static_cast<uint8_t>(ch), caller, slot.owner);
        return false;
    }

    slot.value = val;
    return true;
}


// =============================================================================
// 3. HEADER FIELD WRITE ACCESSORS
// =============================================================================

bool combus_set_runlevel(ComBus& bus, RunLevel rl, ChanOwner caller) {
    if (!_owner_ok(bus.runLevelOwner, caller)) {
        _warn_denied("runLevel", 0xFF, caller, bus.runLevelOwner);
        return false;
    }
    bus.runLevel = rl;
    return true;
}


bool combus_set_keyon(ComBus& bus, bool val, ChanOwner caller) {
    if (!_owner_ok(bus.keyOnOwner, caller)) {
        _warn_denied("keyOn", 0xFF, caller, bus.keyOnOwner);
        return false;
    }
    bus.keyOn = val;
    return true;
}


bool combus_set_battlow(ComBus& bus, bool val, ChanOwner caller) {
    if (!_owner_ok(bus.battLowOwner, caller)) {
        _warn_denied("battLow", 0xFF, caller, bus.battLowOwner);
        return false;
    }
    bus.batteryIsLow = val;
    return true;
}


// EOF combus_access.cpp
