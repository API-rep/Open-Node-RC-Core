/******************************************************************************
 * @file  combus_access.h
 * @brief ComBus accessors: every write checked against the layer of the channel.
 *
 * @details Every write to a channel goes through here.  The writer gives its layer
 *   (ChanLayer), compared with the layer of the channel (rules in combus_defs.h): a
 *   write that is not allowed is skipped and returns false.  Every denied write is
 *   counted (combus_denied_writes()) and logged once per channel by sys_log_warn()
 *   (DEBUG_SYSTEM builds).
 *
 *   Analog channels have two families of accessors:
 *   - application, combus_get_analog() and combus_set_analog(): the logical value
 *     (int32_t).  For a channel declared `zeroAt: neutral` (AnalogComBus::zeroAtNeutral)
 *     the logical zero is CbusNeutral, so the value may be negative; for any other
 *     channel, logical = raw.  The family of the application code (main loop,
 *     dashboards...);
 *   - system, combus_get_raw_analog() and combus_set_raw_analog(): the raw uint16_t,
 *     as stored and sent on the wire, never offset.  The family of the transport
 *     (codec, reception, links), of the input drivers (sticks) and of the chain runner.
 *
 *   TODO(Simulation chain): whether the processors work on the logical value is
 *   decided there; the chain runner stays raw until then.
 *****************************************************************************/
#pragma once

#include <optional>
#include <core/system/combus/combus_defs.h>



// =============================================================================
// 1. ANALOG CHANNELS
// =============================================================================

/**
 * @brief Write the raw value of an analog channel, with no offset (system side).
 *
 * @param bus     Target ComBus.
 * @param ch      Channel.
 * @param val     New raw value.
 * @param caller  Layer of the writer, checked against the layer of the channel.
 *
 * @return true if the write was accepted, false if the layer refused it.
 */

bool combus_set_raw_analog(ComBus& bus, AnalogComBusID ch, uint16_t val, ChanLayer caller);


	/// Optional channel: no write, and false, when it is absent.
inline bool combus_set_raw_analog(ComBus& bus, const std::optional<AnalogComBusID>& ch,
                                  uint16_t val, ChanLayer caller)
{
	if (!ch.has_value()) return false;

	return combus_set_raw_analog(bus, ch.value(), val, caller);
}


/**
 * @brief Read the raw value of an analog channel, with no offset (system side).
 *
 * @param bus  ComBus.
 * @param ch   Channel.
 *
 * @return The raw uint16_t value.
 */

uint16_t combus_get_raw_analog(const ComBus& bus, AnalogComBusID ch);


/**
 * @brief Write the logical value of an analog channel (application side).
 *
 * @details raw = val, plus CbusNeutral when the channel is `zeroAtNeutral`, bounded to
 *   the raw range [0, CbusMaxVal].  Without `zeroAt: neutral`, a negative value is
 *   bounded to 0.
 *
 * @param bus     Target ComBus.
 * @param ch      Channel.
 * @param val     New logical value.
 * @param caller  Layer of the writer, checked against the layer of the channel.
 *
 * @return true if the write was accepted, false if the layer refused it.
 */

bool combus_set_analog(ComBus& bus, AnalogComBusID ch, int32_t val, ChanLayer caller);


/**
 * @brief Read the logical value of an analog channel (application side).
 *
 * @details raw minus CbusNeutral when the channel is `zeroAtNeutral` (range
 *   -CbusNeutral..CbusMaxVal-CbusNeutral), else raw (0..CbusMaxVal).
 *
 * @param bus  ComBus.
 * @param ch   Channel.
 *
 * @return The logical value.
 */

int32_t combus_get_analog(const ComBus& bus, AnalogComBusID ch);



// =============================================================================
// 2. DIGITAL CHANNELS
// =============================================================================

/**
 * @brief Write a digital channel.
 *
 * @details For a both_or channel, the value written is this board's own term: the
 *   channel reads it OR the values received from the live links.
 *
 * @param bus     Target ComBus.
 * @param ch      Channel.
 * @param val     New value.
 * @param caller  Layer of the writer, checked against the layer of the channel.
 *
 * @return true if the write was accepted, false if the layer refused it.
 */

bool combus_set_digital(ComBus& bus, DigitalComBusID ch, bool val, ChanLayer caller);


	/// Optional channel: no write, and false, when it is absent.
inline bool combus_set_digital(ComBus& bus, const std::optional<DigitalComBusID>& ch,
                               bool val, ChanLayer caller)
{
	if (!ch.has_value()) return false;

	return combus_set_digital(bus, ch.value(), val, caller);
}


	/// The code of this board writes with the layer BOARD: processors, contributors.
inline bool combus_set_digital(ComBus& bus, DigitalComBusID ch, bool val) {
	return combus_set_digital(bus, ch, val, ChanLayer::BOARD);
}


/**
 * @brief This board's own term of a both_or channel.
 *
 * @details false while this board has never written the channel itself: a board that
 *   only receives a both_or channel (diffused copy, default value) relays the values
 *   of the other boards but adds nothing of its own.
 *
 * @param slot  The digital channel.
 *
 * @return The last value written by this board, or false if it never wrote it.
 */

inline bool combus_both_or_local(const DigitalComBus& slot) {
	return slot.bothOr.written && slot.bothOr.local;
}


/**
 * @brief Refresh the received term of a both_or channel, and recompute its value.
 *
 * @details value = (this board's own term) OR @p remote.  Called once per cycle by
 *   com_rx_update(), with the OR of the values received from the live links (see
 *   protocol/both_or.md, anti ping-pong).
 *
 * @param bus     ComBus of the node.
 * @param ch      A both_or digital channel.
 * @param remote  OR of the values received from the live links.
 */

void combus_both_or_set_remote(ComBus& bus, DigitalComBusID ch, bool remote);



// =============================================================================
// 3. DENIED WRITES
// =============================================================================

/**
 * @brief Number of writes denied by the layer check since boot.
 *
 * @details A non-zero count means that some writer gives the wrong layer: its value
 *   never reaches the bus.  The first denial of each channel is also logged.
 *
 * @return Denied writes since boot, analog and digital.
 */

uint32_t combus_denied_writes();

// EOF combus_access.h
