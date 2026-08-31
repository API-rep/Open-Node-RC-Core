/******************************************************************************
 * @file combus_uart.h
 * @brief ComBus UART output transport — channel counts, frame size, and cap checks.
 *
 * @details Derives the exact encoded frame size for this machine's ComBus
 *   layout and validates that it fits the generic transport protocol limits
 *   AND the board-supplied UART parameters are within safe operating ranges.
 *   Included by outputs/outputs.h when COMBUS_UART_TX=N or COMBUS_UART=N is set.
 *
 *   Constants exported:
 *     ComBusUartFrameSize  — exact frame size for this layout (bytes)
 *     ComBusUartBaud       — negotiated UART baud rate (≤ board UartMaxBaud)
 *     ComBusUartTxHz       — frame transmit rate in Hz
 *     ComBusUartMaxTxHz    — controller-side frame-rate ceiling (Hz)
 ******************************************************************************/
#pragma once

#include <stdint.h>
#include <core/system/combus/frame/combus_frame.h>
#include <core/config/machines/machine_type.h>        // AnalogComBusRemoteID, DigitalComBusRemoteID (transitive via combus_remote.h)

/// UART transport physical cap — chosen as uint8_t safety ceiling (no hardware limit).
static constexpr uint8_t CombusPhysUartMax = 255u;


// =============================================================================
// 2. EXACT FRAME SIZE FOR THIS COMBUS LAYOUT
// =============================================================================

/**
 * @brief Encoded frame size for this machine's ComBus — in bytes.
 *
 * @details Sized to the actual channel counts, not the protocol maximum.
 *   Smaller than CombusTransportMaxFrame when fewer than the protocol
 *   maximum channels are active. Recalculates automatically when CH_COUNT
 *   values change.
 *
 *   Frame layout:
 *     6 bytes — fixed header (SOF + n_analog + n_dig_bytes + seq + run_level + flags)
 *     ceil(DigitalComBusRemoteID::CH_COUNT / 8) — digital channels packed LSB-first
 *     AnalogComBusRemoteID::CH_COUNT × 2          — analog channels as uint16_t LE
 *     1 byte  — CRC-8
 */


// NOTE (temporaire) : la taille de trame est actuellement calculée sur la
// seule base du contrat REMOTE (CH_COUNT), en supposant que tout canal REMOTE
// est systématiquement transmis. C'est l'hypothèse la plus simple et sûre
// pour l'instant. Cette hypothèse devra être revue lorsque des flags
// d'émission conditionnels seront introduits (ex: retour télémétrique
// uniquement, sous-ensembles de canaux selon le mode de trame) — la taille
// de trame deviendra alors variable et ne pourra plus être un simple constexpr
// basé sur CH_COUNT. Voir COMBUS_PROCESSORS_ROADMAP.md pour le contexte plus
// large.
static constexpr uint8_t ComBusUartFrameSize =
    CombusFrameHeaderLen
  + ((static_cast<uint8_t>(DigitalComBusRemoteID::CH_COUNT) + 7u) / 8u)
  +  (static_cast<uint8_t>(AnalogComBusRemoteID::CH_COUNT)  * 2u)
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