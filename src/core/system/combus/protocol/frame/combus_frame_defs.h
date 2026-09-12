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
// 1. COMBUS TRANSPORT FRAME STRUCTURE
// =============================================================================

/* @brief Static com-bus layout descriptor — channel counts for this frame.
 *
 * @details Set once at init, never changes at runtime. Shared between the TX
 *   codec, RX codec, and protocol modules (combus_tx, combus_rx) to avoid
 *   repeating the two values in every function signature.
 *
 *   On the machine node, built from compile-time constants:
 *   @code
 *     constexpr ComBusFrameCfg comBusFrameCfg = {
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
 *   directly to the first 2 wire bytes after SOF (nAnalog, nDigital).
 *   Any change to field order, type or count **breaks the binary wire protocol**
 *   and requires a matching update in both encode (combus_frame.cpp) and the
 *   memcpy-based decode path. Do not reorder or extend without updating both.
 */

struct ComBusFrameCfg {
  uint8_t nAnalog;   ///< Number of analog channels in this layout.
  uint8_t nDigital;  ///< Number of digital channels in this layout.
};



/* @brief Combus transmission frame header fields shared with the decoded frame.
 *
 * @details These fields are transmitted in the transmission frame immediately
 *   after the SOF byte (offset 0). The same struct is embedded in ComBusFrame,
 *   giving a single source of truth for both wire layout and decoded representation.
 *
 *   Wire byte order (offsets 1–4 after SOF):
 *     0: nAnalog   1: nDigital   2: seq   3: flags
 *
 *   The first two bytes map directly to `ComBusFrameCfg`, enabling a
 *   memcpy-based decode without manual field extraction.
 *
 * @note `seq == 0` is RESERVED on every ComBus transport — generic to all
 *   participants (RF node or serial board), NOT limited to the machine
 *   <-> remote link.  See `combus_handshake.h` for the dedicated
 *   structurally-separate decoder that will pick it up once the
 *   handshake / versioning mechanism is implemented.
 *
 * @note RL2: `runLevel` is NOT a header field — it travels as a standard
 *   combus channel (RUNLEVEL, scope LOCAL, see runlevel.cb). The header
 *   is now 4 bytes (was 5 before RL2). The wire contract is broken on
 *   purpose: any peer still sending the old 5-byte header will be
 *   rejected by the static_assert in combus_frame.h.

 */

struct CombusFrameHeader {
    ComBusFrameCfg cfg;   ///< Static layout snapshot: nAnalog, nDigital (wire offsets 0–1).
    uint8_t seq;          ///< Rolling frame counter (1..255 for control frames; 0 is RESERVED for handshake — see combus_handshake.h).
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
    CombusFrameHeader header;   ///< Wire header fields (offsets 1–4 of the frame, RL2: was 1–5).
    uint16_t*         analog;   ///< Caller-provided buffer (≥ header.nAnalog entries)
    bool*             digital;  ///< Caller-provided buffer (≥ header.nDigital entries)
};

// EOF outputs_struct.h