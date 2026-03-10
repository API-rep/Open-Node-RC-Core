/*!****************************************************************************
 * @file  outputs_struct.h
 * @brief Output transport structure definitions.
 *
 * @details Pure type definitions — no feature flag dependency.
 *   Available regardless of active output modules.
 *   Specific constants and static_asserts are in the config sub-files:
 *   src/core/config/outputs/
 *******************************************************************************///
#pragma once

#include <stdint.h>
#include <stdbool.h>


// =============================================================================
// 1. UART OUTPUT TRANSPORT
// =============================================================================

/**
 * @brief Hardware and rate configuration for one UART output transport link.
 *
 * @details Groups the four parameters required by any UART-based output
 *   initialisation into a single typed aggregate.
 *   Constexpr instances are defined in the active output config sub-file
 *   (e.g. src/core/config/outputs/sound_uart.h).
 */
struct OutputUartConfig {
  int8_t   txPin;   ///< ESP32 GPIO TX pin.
  int8_t   rxPin;   ///< ESP32 GPIO RX pin (-1 = TX-only link).
  uint32_t baud;    ///(<  to UART baud rate (Hz).
  uint32_t txHz;    ///< Frame transmit rate (Hz).
};



// =============================================================================
// 2. COMBUS TRANSPORT FRAME STRUCTURE
// =============================================================================

/**
 * @brief Combus transmission frame header fields shared with the decoded frame.
 *
 * @details These fields are transmitted in the transmission frame immediately
 * after the SOF byte (offset 0). The same struct is embedded in ComBusFrame,
 * giving a single source of truth for both wire layout and decoded representation.
 */

struct CombusFrameHeader {
    uint8_t envId;      ///< MACHINE_TYPE numeric value (see combus_types.h layout)
    uint8_t seq;        ///< Rolling frame counter (0 to 255
    uint8_t runLevel;   ///< Combus RunLevel cast to uint8_t
    uint8_t flags;      ///< COMBUS_FLAG_* bits (transport status only)
    uint8_t nAnalog;    ///< Number of analog channels transmitted
    uint8_t nDigital;   ///< Number of digital channels transmitted
};



/**
 * @brief Decoded ComBus frame — header fields + pointers to the decoded payload.
 *
 * @details `analog` and `digital` point to the decoded analog and digital data.
 * Their effective sizes are `header.nAnalog` and `header.nDigital`, which are
 * themselves derived from `AnalogComBusID::CH_COUNT` / `DigitalComBusID::CH_COUNT`
 * defined in the machine's combus layout (combus_types.h).
 *
 * Correct decoding and application of the ComBus state is guaranteed by the
 * fact that both nodes (machine and receiver) share the same combus_types.h.
 */
struct ComBusFrame {
    CombusFrameHeader header;   ///< Wire header fields (offsets 1–6 of the frame).
    uint16_t*         analog;   ///< Caller-provided buffer (≥ header.nAnalog entries)
    bool*             digital;  ///< Caller-provided buffer (≥ header.nDigital entries)
};

// EOF outputs_struct.h
