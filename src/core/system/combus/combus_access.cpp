/******************************************************************************
 * @file  combus_access.cpp
 * @brief ComBus accessors: every write checked against the layer of the channel.
 *
 * @details A denied write is skipped, counted, and logged the first time for its
 *   channel: a wrong writer layer cannot stay silent.
 *****************************************************************************/

#include "combus_access.h"

#include <core/system/debug/logging/debug.h>
#include <core/system/combus/combus_res.h>   // CbusNeutral, CbusMaxVal



// =============================================================================
// 1. INTERNAL HELPERS
// =============================================================================

/**
 * @brief Layer check. Says whether a writer may write a channel.
 *
 * @details In order:
 *   1. UNDEFINED: nobody, for safety.
 *   2. BASE: every writer.
 *   3. SYSTEM: the boards of this node, writer SYSTEM or BOARD.
 *   4. BOARD: this board only, writer BOARD.
 *
 * @param slot_layer    Layer of the channel.
 * @param caller_layer  Layer of the writer.
 *
 * @return true if the write is allowed.
 */

static inline bool layerOk(ChanLayer slot_layer, ChanLayer caller_layer) {
		// 1. UNDEFINED
	if (slot_layer == ChanLayer::UNDEFINED) {
		return false;
	}

		// 2. BASE
	if (slot_layer == ChanLayer::BASE) {
		return true;
	}

		// 3. SYSTEM
	if (slot_layer == ChanLayer::SYSTEM) {
		return (caller_layer == ChanLayer::SYSTEM) || (caller_layer == ChanLayer::BOARD);
	}

		// 4. BOARD
	if (slot_layer == ChanLayer::BOARD) {
		return caller_layer == ChanLayer::BOARD;
	}

		// Check for a value out of the enum
	return false;
}


	// Denied writes since boot, and one "already reported" bit per channel index and per
	// type: a channel index is a uint8_t, so 32 bytes cover them all.
static uint32_t s_deniedCount = 0u;
static uint8_t  s_reportedAnalog[32]  = {};
static uint8_t  s_reportedDigital[32] = {};


/**
 * @brief Denied write. Counts it, and logs it once per channel.
 *
 * @details The count is always kept.  The log line is printed the first time a given
 *   channel is denied, so that a wrong writer layer shows up without flooding the log.
 *
 * @param label       "analog" or "digital", for the log.
 * @param isAnalog    Selects the "already reported" bits.
 * @param ch          Index of the channel.
 * @param caller      Layer of the writer.
 * @param slot_layer  Layer of the channel.
 */

static void noteDenied(const char* label, bool isAnalog, uint8_t ch,
                       ChanLayer caller, ChanLayer slot_layer) {
	++s_deniedCount;

		// Check whether this channel was already reported
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
// 2. ANALOG CHANNELS
// =============================================================================

	/// The layer first: a denied write leaves the value as it was.
bool combus_link_set_raw_analog(ComBus& bus, AnalogComBusID ch, uint16_t val, ChanLayer layer) {
	auto& slot = bus.analogBus[static_cast<uint8_t>(ch)];

		// Check the layer
	if (!layerOk(slot.layer, layer)) {
		noteDenied("analog", true, static_cast<uint8_t>(ch), layer, slot.layer);
		return false;
	}

	slot.value = val;

	return true;
}


	/// The stored value, as it travels.
uint16_t combus_get_raw_analog(const ComBus& bus, AnalogComBusID ch) {
	return bus.analogBus[static_cast<uint8_t>(ch)].value;
}


	/// Logical to raw: the offset of the logical zero, then the bounds of the raw range.
bool combus_set_analog(ComBus& bus, AnalogComBusID ch, int32_t val) {
	const auto& slot = bus.analogBus[static_cast<uint8_t>(ch)];

	int32_t raw = slot.zeroAtNeutral ? val + static_cast<int32_t>(CbusNeutral) : val;

		// Bound to the raw range
	if (raw < 0)                                  raw = 0;
	if (raw > static_cast<int32_t>(CbusMaxVal))   raw = static_cast<int32_t>(CbusMaxVal);

	return combus_set_raw_analog(bus, ch, static_cast<uint16_t>(raw));
}


	/// Raw to logical: minus CbusNeutral when the channel is zeroAtNeutral.
int32_t combus_get_analog(const ComBus& bus, AnalogComBusID ch) {
	const auto& slot = bus.analogBus[static_cast<uint8_t>(ch)];

	return slot.zeroAtNeutral ? static_cast<int32_t>(slot.value) - static_cast<int32_t>(CbusNeutral)
	                          : static_cast<int32_t>(slot.value);
}



// =============================================================================
// 3. DIGITAL CHANNELS
// =============================================================================

/**
 * @brief Digital write. Checks the layer, then writes the value or the own term.
 *
 * @details In order:
 *   1. The layer: a denied write leaves the channel as it was.
 *   2. both_or: this board's value is kept apart, and the value becomes own OR received.
 *   3. Any other channel: the value.
 */

bool combus_link_set_digital(ComBus& bus, DigitalComBusID ch, bool val, ChanLayer layer) {
	auto& slot = bus.digitalBus[static_cast<uint8_t>(ch)];

		// 1. Layer
	if (!layerOk(slot.layer, layer)) {
		noteDenied("digital", false, static_cast<uint8_t>(ch), layer, slot.layer);
		return false;
	}

		// 2. both_or
	if (slot.bothOr.enabled) {
		slot.bothOr.local   = val;
		slot.bothOr.written = true;
		slot.value          = val || slot.bothOr.remote;
		return true;
	}

		// 3. Value
	slot.value = val;

	return true;
}


	/// No layer check: the received term is computed by the core, never written by a module.
void combus_both_or_set_remote(ComBus& bus, DigitalComBusID ch, bool remote) {
	auto& slot = bus.digitalBus[static_cast<uint8_t>(ch)];

	slot.bothOr.remote = remote;
	slot.value         = combus_both_or_local(slot) || remote;
}


	/// Analog and digital together.
uint32_t combus_denied_writes() {
	return s_deniedCount;
}

// EOF combus_access.cpp
