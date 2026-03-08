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
  uint32_t baud;    ///< UART baud rate (Hz).
  uint32_t txHz;    ///< Frame transmit rate (Hz).
};

// =============================================================================
// 2. COMBUS TRANSPORT FRAME STRUCTURE
// =============================================================================

/**
 * @brief Decoded ComBus frame — lightweight value-only representation.
 *
 * @details Used by the receiver side after decoding a transport frame.
 * The receiver maps these values back onto its local ComBus arrays.
 *
 * `analog` and `digital` are caller-owned pointers — the caller allocates
 * backing arrays (static or module-level) before passing the snapshot to
 * combus_transport_decode(). This mirrors the ComBus pattern and avoids
 * over-allocating fixed-size buffers when the machine uses fewer channels.
 *
 * Buffer capacity is passed directly to `combus_frame_decode()` as explicit
 * parameters — no need to embed compile-time constants into the struct.
 *
 * There are no per-type protocol caps — the only constraint is that the
 * total serialized frame fits within the active physical transport
 * (see src/core/config/outputs/ for per-transport caps).
 *
 * Typical setup (receiver module init):
 * @code
 *   static uint16_t analogBuf[N_ANA];
 *   static bool     digitalBuf[N_DIG];
 *   snap.analog  = analogBuf;
 *   snap.digital = digitalBuf;
 *   combus_frame_decode(&snap, buf, len, N_ANA, N_DIG);
 * @endcode
 */
typedef struct {
    uint8_t   envId;      ///< Machine type ID (matches sender's MACHINE_TYPE)
    uint8_t   seq;        ///< Sequence number
    uint8_t   runLevel;   ///< RunLevel as uint8_t
    uint8_t   flags;      ///< COMBUS_FLAG_* bits
    uint8_t   nAnalog;    ///< Number of analog values written into analog[]
    uint8_t   nDigital;   ///< Number of digital values written into digital[]
    uint16_t* analog;     ///< Caller-provided buffer (≥ nAnalog entries)
    bool*     digital;    ///< Caller-provided buffer (≥ nDigital entries)
} ComBusFrame;

// EOF outputs_struct.h
