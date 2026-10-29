/******************************************************************************
 * @file combus_access.h
 * @brief Layer-checked write accessors for ComBus channels.
 *
 * @details Each write function compares the caller's `ChanLayer` against the
 *   channel's declared `layer` before writing. If the caller's layer does not
 *   have sufficient privileges, the write is skipped and returns `false`.
 *   Every denied write is counted (combus_denied_writes()) and reported once per
 *   channel through sys_log_warn() (DEBUG_SYSTEM builds).
 *
 *   This layer is optional — existing direct field writes remain valid during
 *   the migration. New code should use these accessors so layer violations
 *   are visible at runtime.
 *
 *   Rules:
 *   - UNDEFINED → write denied for all callers (safety)
 *   - BOARD     → only local firmware caller may write
 *   - SYSTEM    → local firmware caller + extension boards caller of same node)
 *   - BASE      → any source (local + extensions + remote nodes caller)
 *****************************************************************************/
#pragma once

#include <optional>
#include <core/system/combus/combus_defs.h>

// Forward declaration only — AnalogComBusID/DigitalComBusID used
// below solely as type parameters for combus_set function.
enum class AnalogComBusID  : uint8_t;
enum class DigitalComBusID : uint8_t;

// =============================================================================
// 1. CHANNEL WRITE ACCESSORS
// =============================================================================

/*
 *   Analog channels have two families of accessors (link rework S3-4):
 *   - application: combus_get_analog() / combus_set_analog() — the LOGICAL
 *     value (int32_t).  For a channel declared `zeroAt: neutral`
 *     (AnalogComBus::zeroAtNeutral) the logical zero is CbusNeutral, so the
 *     value may be negative; for any other channel logical = raw.  This is
 *     the family to use in application code (main loop, dashboards…).
 *   - system: combus_get_raw_analog() / combus_set_raw_analog() — the RAW
 *     uint16_t, as stored and sent on the wire, never offset.  Reserved to
 *     the transport (codec, RX, links), the input drivers (sticks) and the
 *     chain runner (until the processors are decided, simulation WIP).
 */

/**
 * @brief Write the raw value of an analog ComBus channel (system side, no offset).
 * @param bus     Target ComBus instance.
 * @param ch      Channel index (typed enum, machine-specific).
 * @param val     New raw value.
 * @param caller  Layer of the calling module — checked against ch.layer.
 * @return true if the write was accepted, false if layer mismatch.
 */

bool combus_set_raw_analog(ComBus& bus, AnalogComBusID ch, uint16_t val, ChanLayer caller);

  /// Write the raw value of an optional analog channel — no-op (returns false) if absent.
inline bool combus_set_raw_analog(ComBus& bus, const std::optional<AnalogComBusID>& ch,
                                  uint16_t val, ChanLayer caller)
{
    if (!ch.has_value()) return false;

    return combus_set_raw_analog(bus, ch.value(), val, caller);
}


/**
 * @brief Read the raw value of an analog ComBus channel (system side, no offset).
 * @param bus  ComBus instance.
 * @param ch   Channel index.
 * @return     The raw uint16_t value.
 */

uint16_t combus_get_raw_analog(const ComBus& bus, AnalogComBusID ch);


/**
 * @brief Write the logical value of an analog ComBus channel (application side).
 *
 * @details raw = val, plus CbusNeutral when the channel is `zeroAtNeutral`,
 *   bounded to the raw range [0, CbusMaxVal].  Without `zeroAt: neutral` a
 *   negative value is bounded to 0.
 *
 * @param bus     Target ComBus instance.
 * @param ch      Channel index.
 * @param val     New logical value.
 * @param caller  Layer of the calling module — checked against ch.layer.
 * @return true if the write was accepted, false if layer mismatch.
 */

bool combus_set_analog(ComBus& bus, AnalogComBusID ch, int32_t val, ChanLayer caller);


/**
 * @brief Read the logical value of an analog ComBus channel (application side).
 *
 * @details raw, minus CbusNeutral when the channel is `zeroAtNeutral`
 *   (range -CbusNeutral..CbusMaxVal-CbusNeutral), else raw (0..CbusMaxVal).
 *
 * @param bus  ComBus instance.
 * @param ch   Channel index.
 * @return     The logical value.
 */

int32_t combus_get_analog(const ComBus& bus, AnalogComBusID ch);


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



/**
 * @brief This board's own contribution to a both_or channel.
 *
 * @details false while this board has never written the channel itself: a
 *   board that only receives a both_or channel (diffused copy, default value)
 *   relays the values of the other boards but adds nothing of its own.
 *
 * @param slot  Digital channel slot.
 * @return      The last locally written value, or false if never written.
 */

inline bool combus_both_or_local(const DigitalComBus& slot) {
    return slot.bothOr.written && slot.bothOr.local;
}


/**
 * @brief Refresh the received term of a both_or channel and recompute its value.
 *
 * @details value = (this board's own value) OR @p remote. Called once per cycle
 *   by com_rx_update() with the OR of the values received from the live links
 *   (see protocol/both_or.md, anti ping-pong).
 *
 * @param bus     Node ComBus.
 * @param ch      A both_or digital channel.
 * @param remote  OR of the values received from the live links.
 */

void combus_both_or_set_remote(ComBus& bus, DigitalComBusID ch, bool remote);


/**
 * @brief Number of writes denied by the layer check since boot.
 *
 * @details A non-zero count means some caller writes a channel with the wrong
 *   layer: the value never reaches the bus. The first denial of each channel
 *   is also logged by sys_log_warn() (DEBUG_SYSTEM builds).
 *
 * @return  Denied writes since boot (all channels, analog and digital).
 */

uint32_t combus_denied_writes();



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

// RL3: combus_set_runlevel() removed — runLevel is now a plain analog channel (RUNLEVEL).
//       Write it via combus_set_analog(bus, AnalogComBusID::RUNLEVEL, (int32_t)rl, ChanLayer::SYSTEM).


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
// 4. INTERNAL USAGE OVERLOADS (BOARD-level access)
// =============================================================================

/**
 * @brief Internal overload for processors running with BOARD-level access.
 * @details Used by CbProcFn implementations (local firmware).
 */
inline bool combus_set_digital(ComBus& bus, DigitalComBusID ch, bool val) {
    return combus_set_digital(bus, ch, val, ChanLayer::BOARD);
}

// RL3: inline combus_set_runlevel() overload removed — runLevel is a plain analog channel.

// EOF combus_access.h
