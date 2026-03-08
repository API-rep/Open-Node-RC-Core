/******************************************************************************
 * @file sound_uart.h
 * @brief Sound UART output transport — channel counts, frame size, and cap checks.
 *
 * @details Derives the exact encoded frame size for this machine’s ComBus
 *   layout and validates that it fits the generic transport protocol limits
 *   AND the board-supplied UART parameters are within safe operating ranges.
 *   Included by outputs/outputs.h when SOUND_OUTPUT_UART is set.
 *
 *   Constants exported:
 *     SoundTransportNAnalog    — active analog channel count (from combus)
 *     SoundTransportNDigital   — active digital channel count (from combus)
 *     SoundTransportFrameSize  — exact frame size for this layout (bytes)
 *     SoundTransportMaxTxHz    — controller-side frame-rate ceiling (Hz)
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <core/utils/combus/combus_frame.h>
#include <core/config/combus/combus_types.h>    // AnalogComBusID, DigitalComBusID
// Board constants (SoundUartBaud, SoundTransportTxHz) available via outputs/outputs.h → boards/boards.h.

/// UART transport physical cap — chosen as uint8_t safety ceiling (no hardware limit).
static constexpr uint8_t CombusPhysUartMax = 255u;


// =============================================================================
// 1. CHANNEL COUNTS  (derived from active combus config)
// =============================================================================

	/// Analog channels transmitted to the sound node — matches this machine’s ComBus layout.
static constexpr uint8_t SoundTransportNAnalog  = static_cast<uint8_t>(AnalogComBusID::CH_COUNT);

	/// Digital channels transmitted to the sound node — matches this machine’s ComBus layout.
static constexpr uint8_t SoundTransportNDigital = static_cast<uint8_t>(DigitalComBusID::CH_COUNT);


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
 *     ceil(SoundTransportNDigital / 8) — digital channels packed LSB-first
 *     SoundTransportNAnalog × 2       — analog channels as uint16_t LE
 *     1 byte  — CRC-8
 */
static constexpr uint8_t SoundTransportFrameSize =
    7u
  + ((SoundTransportNDigital + 7u) / 8u)
  +  (SoundTransportNAnalog  * 2u)
  + 1u;


// =============================================================================
// 3. BOARD TRANSPORT PARAMETERS
// =============================================================================

  // Controller-side frame-rate ceiling (Hz).
  // TODO: replace with SoundRxMaxHz from sound_config.h once sound node
  //       firmware exposes its real receive throughput as a constexpr.
static constexpr uint32_t SoundTransportMaxTxHz = 200u;


// =============================================================================
// 4. COMPILE-TIME CAP CHECKS
// =============================================================================

  // --- Physical transport caps: frame must fit the active medium ---
static_assert(SoundTransportFrameSize <= CombusPhysUartMax,
              "SoundTransportFrameSize exceeds UART practical cap (CombusPhysUartMax)");

  // --- Board configuration: baud and TX rate must be valid ---
static_assert(SoundUartBaud > 0u,
              "SoundUartBaud must be non-zero (check board header)");

static_assert(SoundTransportTxHz > 0u && SoundTransportTxHz <= SoundTransportMaxTxHz,
              "SoundTransportTxHz out of range [1, SoundTransportMaxTxHz] (check board header)");

// EOF sound_uart.h
