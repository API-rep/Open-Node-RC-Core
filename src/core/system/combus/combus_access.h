/******************************************************************************
 * @file combus_access.h
 * @brief Layer-checked write accessors for ComBus channels.
 *
 * @details Each write function compares the caller's `ChanLayer` against the
 *   channel's declared `layer` before writing. If the caller's layer does not
 *   have sufficient privileges, the write is skipped and returns `false`.
 *   In DEBUG_COMBUS builds a warning is also printed.
 *
 *   This layer is optional — existing direct field writes remain valid during
 *   the migration. New code should use these accessors so layer violations
 *   are visible at runtime.
 *
 *   Rules:
 *   - UNDEFINED → write denied for all callers (safety)
 *   - SYSTEM    → only local firmware caller may write
 *   - LOCAL     → local firmware caller + extension boards caller of same node)
 *   - REMOTE    → any source (local + extensions + remote nodes caller)
 *****************************************************************************/
#pragma once

#include <optional>
#include <struct/combus_struct.h>

// Forward declaration only — AnalogComBusID/DigitalComBusID used
// below solely as type parameters for combus_set function.
enum class AnalogComBusID  : uint8_t;
enum class DigitalComBusID : uint8_t;

// =============================================================================
// 1. CHANNEL WRITE ACCESSORS
// =============================================================================

/**
 * @brief Write a value to an analog ComBus channel.
 * @param bus     Target ComBus instance.
 * @param ch      Channel index (typed enum, machine-specific).
 * @param val     New value to write.
 * @param caller  Layer of the calling module — checked against ch.layer.
 * @return true if the write was accepted, false if layer mismatch.
 */

bool combus_set_analog(ComBus& bus, AnalogComBusID ch, uint16_t val, ChanLayer caller);

  /// Write an optional analog channel — no-op (returns false) if absent.
inline bool combus_set_analog(ComBus& bus, const std::optional<AnalogComBusID>& ch,
                              uint16_t val, ChanLayer caller)
{
    if (!ch.has_value()) return false;

    return combus_set_analog(bus, ch.value(), val, caller);
}


/**
 * @brief Write a value to a digital ComBus channel.
 * @param bus     Target ComBus instance.
 * @param ch      Channel index (typed enum, machine-specific).
 * @param val     New value to write.
 * @param caller  Layer of the calling module — checked against ch.layer.
 * @return true if the write was accepted, false if layer mismatch.
 */

bool combus_set_digital(ComBus& bus, DigitalComBusID ch, bool val, ChanLayer caller);

  /// Write an optional digital channel — no-op (returns false) if absent.
inline bool combus_set_digital(ComBus& bus, const std::optional<DigitalComBusID>& ch,
                               bool val, ChanLayer caller)
{
    if (!ch.has_value()) return false;

    return combus_set_digital(bus, ch.value(), val, caller);
}



// =============================================================================
// 2. HEADER FIELD WRITE ACCESSORS
// =============================================================================

/**
 * @brief Write the ComBus run level.
 * 
 * @param bus    Target ComBus instance.
 * @param rl     New RunLevel value.
 * @param caller Layer of the calling module — checked against bus.runLevelLayer.
 * 
 * @return true if the write was accepted, false if layer mismatch.
 */

bool combus_set_runlevel(ComBus& bus, RunLevel rl, ChanLayer caller);



/**
 * @brief Write the ComBus batteryIsLow flag.
 * 
 * @param bus    Target ComBus instance.
 * @param val    New batteryIsLow value.
 * @param caller Layer of the calling module — checked against bus.battLowLayer.
 * 
 * @return true if the write was accepted, false if layer mismatch.
 */

bool combus_set_battlow(ComBus& bus, bool val, ChanLayer caller);


// =============================================================================
// 3. SIGN-MAGNITUDE HELPERS
// =============================================================================

/**
 * @brief Helpers for sign-magnitude ComBus channel encoding.
 *
 * @details Encodes direction + speed magnitude into a single `uint16_t`:
 *   - bit 15 : direction (1 = REV, 0 = FWD)
 *   - bits 14–0 : magnitude (0..32767)
 *
 *   Used by processors that work in a sign-magnitude pipeline
 *   (e.g. CbAnalog-encoded channels before the direction-strip step).
 */
struct CbAnalog {
    static constexpr uint16_t kDirBit  = 0x8000u; ///< Direction bit (bit 15): 1 = REV, 0 = FWD.
    static constexpr uint16_t kMagMask = 0x7FFFu; ///< Magnitude mask (bits 14-0): 0..32767.

    /// Extract magnitude (0..32767) — independent of direction bit.
    static constexpr uint16_t getSpeed(uint16_t v)              { return v & kMagMask; }

    /// True when the REV direction bit is set.
    static constexpr bool     isRev(uint16_t v)            { return (v & kDirBit) != 0u; }

    /// Encode direction + magnitude into a sign-magnitude uint16_t.
    static constexpr uint16_t encode(bool rev, uint16_t m) { return (rev ? kDirBit : 0u) | (m & kMagMask); }

    /// Convenience: standing value (stopped, FWD direction by default).
    static constexpr uint16_t kStopped = 0u;
};

// =============================================================================
// 4. INTERNAL USAGE OVERLOADS (SYSTEM-level access)
// =============================================================================

/**
 * @brief Internal overload for processors running with SYSTEM-level access.
 * @details Used by CbProcFn implementations (local firmware).
 */
inline bool combus_set_analog(ComBus& bus, AnalogComBusID ch, uint16_t val) {
    return combus_set_analog(bus, ch, val, ChanLayer::SYSTEM);
}

/**
 * @brief Internal overload for processors running with SYSTEM-level access.
 * @details Used by CbProcFn implementations (local firmware).
 */
inline bool combus_set_digital(ComBus& bus, DigitalComBusID ch, bool val) {
    return combus_set_digital(bus, ch, val, ChanLayer::SYSTEM);
}

/**
 * @brief Internal overload for processors running with SYSTEM-level access.
 * @details Used by CbProcFn implementations (local firmware).
 */
inline bool combus_set_runlevel(ComBus& bus, RunLevel rl) {
    return combus_set_runlevel(bus, rl, ChanLayer::SYSTEM);
}

/**
 * @brief Internal overload for processors running with SYSTEM-level access.
 * @details Used by CbProcFn implementations (local firmware).
 */
inline bool combus_set_battlow(ComBus& bus, bool val) {
    return combus_set_battlow(bus, val, ChanLayer::SYSTEM);
}

// EOF combus_access.h
