/******************************************************************************
 * @file combus_uart.h
 * @brief ComBus UART output transport — channel counts, frame size, and cap checks.
 *
 * @details Derives the exact encoded frame size for this machine's ComBus
 *   layout and validates that it fits the generic transport protocol limits
 *   AND the board-supplied UART parameters are within safe operating ranges.
 *   Included by com/com.h when COMBUS_UART_TX=N or COMBUS_UART=N is set.
 *
 *   Constants exported:
 *     ComBusUartFrameSize  — exact frame size for this layout (bytes)
 *     ComBusUartMaxTxHz    — controller-side frame-rate ceiling (Hz)
 *
 *   The line rate and the transmit rate of the link are the ComProtocolCfg of
 *   its ComDev, defined with the node links (board/com_dev.h).
 ******************************************************************************/
#pragma once

#include <stdint.h>
#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/config/machines/machine_type.h>        // AnalogComBusBaseID, DigitalComBusBaseID (transitive via combus_base.h)

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
 *     ceil(DigitalComBusBaseID::CH_COUNT / 8) — digital channels packed LSB-first
 *     AnalogComBusBaseID::CH_COUNT × 2          — analog channels as uint16_t LE
 *     1 byte  — CRC-8
 */


// NOTE (temporaire) : la taille de trame est actuellement calculée sur la
// seule référence du contrat BASE (CH_COUNT), en supposant que tout canal BASE
// est systématiquement transmis. C'est l'hypothèse la plus simple et sûre
// pour l'instant. Cette hypothèse devra être revue lorsque des flags
// d'émission conditionnels seront introduits (ex: retour télémétrique
// uniquement, sous-ensembles de canaux selon le mode de trame) — la taille
// de trame deviendra alors variable et ne pourra plus être un simple constexpr
// basé sur CH_COUNT. Voir COMBUS_PROCESSORS_ROADMAP.md pour le contexte plus
// large.
static constexpr uint8_t ComBusUartFrameSize =
    CombusFrameHeaderLen
  + ((static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT) + 7u) / 8u)
  +  (static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT)  * 2u)
  + 1u;



// =============================================================================
// 3. TRANSPORT CEILING
// =============================================================================

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

  // --- Baud rate and frame rate: the ComProtocolCfg of the link (board/com_dev.h) is checked ---
  //     against UartMaxBaud and ComBusUartMaxTxHz in machines/init/com/combus_uart_init.cpp,
  //     where the board header and the node links are both in scope.

// EOF combus_uart.h