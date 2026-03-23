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
 *   (e.g. src/core/config/outputs/combus_uart.h).
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
 * @brief Static com-bus layout descriptor — machine identity and channel counts.
 *
 * @details Set once at init, never changes at runtime. Shared between the TX
 *   codec, RX codec, and protocol modules (combus_tx, combus_rx) to avoid
 *   repeating the three values in every function signature.
 *
 *   On the machine node, built from compile-time constants:
 *   @code
 *     constexpr ComBusFrameCfg comBusFrameCfg = {
 *       .envId    = static_cast<uint8_t>(MACHINE_TYPE),
 *       .nAnalog  = static_cast<uint8_t>(AnalogComBusID::CH_COUNT),
 *       .nDigital = static_cast<uint8_t>(DigitalComBusID::CH_COUNT),
 *     };
 *   @endcode
 *
 *   On the remote node, one instance per managed machine (no using namespace —
 *   the remote keeps explicit DumperTruck:: / AutreMachine:: prefixes to
 *   distinguish layouts).
 *
 * @warning This struct is the **first member** of CombusFrameHeader and maps
 *   directly to the first 3 wire bytes after SOF (envId, nAnalog, nDigital).
 *   Any change to field order, type or count **breaks the binary wire protocol**
 *   and requires a matching update in both encode (combus_frame.cpp) and the
 *   memcpy-based decode path. Do not reorder or extend without updating both.
 */

struct ComBusFrameCfg {
  uint8_t envId;     ///< MACHINE_TYPE numeric value — identifies the machine layout.
  uint8_t nAnalog;   ///< Number of analog channels in this layout.
  uint8_t nDigital;  ///< Number of digital channels in this layout.
};



/**
 * @brief Combus transmission frame header fields shared with the decoded frame.
 *
 * @details These fields are transmitted in the transmission frame immediately
 *   after the SOF byte (offset 0). The same struct is embedded in ComBusFrame,
 *   giving a single source of truth for both wire layout and decoded representation.
 *
 *   Wire byte order (offsets 1–6 after SOF):
 *     0: envId   1: nAnalog   2: nDigital   3: seq   4: runLevel   5: flags
 *
 *   The first three bytes map directly to `ComBusFrameCfg`, enabling a
 *   memcpy-based decode without manual field extraction.
 */

struct CombusFrameHeader {
    ComBusFrameCfg cfg;   ///< Static layout snapshot: envId, nAnalog, nDigital (wire offsets 0–2).
    uint8_t seq;          ///< Rolling frame counter (0 to 255).
    uint8_t runLevel;     ///< Combus RunLevel cast to uint8_t.
    uint8_t flags;        ///< COMBUS_FLAG_* bits (transport status only).
};



/**
 * @brief Decoded ComBus frame — header fields + pointers to the decoded payload.
 *
 * @details `analog` and `digital` point to the decoded analog and digital data.
 *   Their effective sizes are `header.nAnalog` and `header.nDigital`, which are
 *   themselves derived from `AnalogComBusID::CH_COUNT` / `DigitalComBusID::CH_COUNT`
 *   defined in the machine's combus layout (combus_types.h).
 *   
 *   Correct decoding and application of the ComBus state is guaranteed by the
 *   fact that both nodes (machine and receiver) share the same combus_types.h.
 */

struct ComBusFrame {
    CombusFrameHeader header;   ///< Wire header fields (offsets 1–6 of the frame).
    uint16_t*         analog;   ///< Caller-provided buffer (≥ header.nAnalog entries)
    bool*             digital;  ///< Caller-provided buffer (≥ header.nDigital entries)
};

// EOF outputs_struct.h
