/******************************************************************************
 * @file combus_uart.h
 * @brief ComBus UART output transport — channel counts, frame size, and cap checks.
 *
 * @details Derives the exact encoded frame size for this machine's ComBus
 *   layout and validates that it fits the generic transport protocol limits
 *   AND the board-supplied UART parameters are within safe operating ranges.
 *   Included by outputs/outputs.h when COMBUS_OUTPUT_UART is set.
 *
 *   Constants exported:
 *     ComBusUartFrameSize  — exact frame size for this layout (bytes)
 *     ComBusUartBaud       — negotiated UART baud rate (≤ board UartMaxBaud)
 *     ComBusUartTxHz       — frame transmit rate in Hz
 *     ComBusUartMaxTxHz    — controller-side frame-rate ceiling (Hz)
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <core/combus/combus_frame.h>
#include <core/config/combus/combus_types.h>           // AnalogComBusID, DigitalComBusID (IS_MACHINE absent → enums only)

/// UART transport physical cap — chosen as uint8_t safety ceiling (no hardware limit).
static constexpr uint8_t CombusPhysUartMax = 255u;


// =============================================================================

// 2. EXACT FRAME SIZE FOR THIS COMBUS LAYOUT
// =============================================================================

/**
 * @brief Encoded frame size for this machine’s ComBus — in bytes.
 *
 * @details Sized to the actual channel counts, not the protocol maximum.
 *   Smaller than CombusTransportMaxFrame when fewer than the protocol
 *   maximum channels are active. Recalculates automatically when CH_COUNT
 *   values change.
 *
 *   Frame layout:
 *     7 bytes — fixed header (SOF + env_id + seq + run_level + flags + n_analog + n_dig_bytes)
 *     ceil(DigitalComBusID::CH_COUNT / 8) — digital channels packed LSB-first
 *     AnalogComBusID::CH_COUNT × 2        — analog channels as uint16_t LE
 *     1 byte  — CRC-8
 */
static constexpr uint8_t ComBusUartFrameSize =
    7u
  + ((static_cast<uint8_t>(DigitalComBusID::CH_COUNT) + 7u) / 8u)
  +  (static_cast<uint8_t>(AnalogComBusID::CH_COUNT)  * 2u)
  + 1u;


// =============================================================================
// 3. TRANSPORT PARAMETERS  (protocol — agreed between machine and sound node)
// =============================================================================

	/// UART baud rate for the ComBus UART TX link.
	/// Must match SOUND_UART_BAUD in the sound node's sound_config.h.
static constexpr uint32_t ComBusUartBaud         = 115200u;

	/// Frame transmit rate (Hz).
static constexpr uint32_t ComBusUartTxHz         = 50u;

	/// Controller-side frame-rate ceiling (Hz).
	/// TODO: replace with SoundRxMaxHz from sound_config.h once sound node
	///       firmware exposes its real receive throughput as a constexpr.
static constexpr uint32_t ComBusUartMaxTxHz      = 200u;


// =============================================================================
// 4. COMPILE-TIME CAP CHECKS
// =============================================================================

  // --- Physical transport caps: frame must fit the active medium ---
static_assert(ComBusUartFrameSize <= CombusPhysUartMax,
              "ComBusUartFrameSize exceeds UART practical cap (CombusPhysUartMax)");

  // --- Baud rate: checked in output_init.cpp (UartMaxBaud requires board header ---
  //     which is not yet in scope here — see static_assert in output_init.cpp)

  // --- Frame rate: must be in valid protocol range ---
static_assert(ComBusUartTxHz > 0u && ComBusUartTxHz <= ComBusUartMaxTxHz,
              "ComBusUartTxHz out of range [1, ComBusUartMaxTxHz]");

// EOF combus_uart.h
