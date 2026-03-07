/******************************************************************************
 * @file combus_transport.h
 * Generic ComBus binary transport — frame encoding / decoding.
 *
 * @details Serializes a ComBus snapshot into a compact binary frame suitable
 * for both UART point-to-point links (sound module) and ESP-Now RF
 * broadcast (remote ↔ machine).
 *
 * Frame layout (variable length, max COMBUS_TRANSPORT_MAX_FRAME bytes):
 * @code
 *  Offset  Size  Field
 *  ------  ----  -----
 *   0       1    SOF          0xAA
 *   1       1    env_id       MACHINE_TYPE numeric value (identifies ComBus layout)
 *   2       1    seq          rolling counter 0-255
 *   3       1    run_level    RunLevel cast to uint8_t
 *   4       1    flags        bit0=batteryIsLow  bit1=keyOn  bit2=failSafe
 *   5       1    n_analog     number of analog channels (≤ COMBUS_MAX_ANALOG)
 *   6       1    n_dig_bytes  ceil(n_digital / 8)
 *   7       var  digital[]    digital bits packed LSB-first
 *   7+d     var  analog[]     uint16_t LE × n_analog
 *   last    1    crc8         CRC-8/MAXIM over bytes [0 … last-1]
 * @endcode
 *
 * @note analogBusMaxVal is a static build-time constant — not transmitted.
 *       The receiver must share the same MACHINE_TYPE to interpret values.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <struct/combus_struct.h>


// =============================================================================
// 1. CONSTANTS
// =============================================================================

/// Start-of-frame sentinel byte.
#define COMBUS_TRANSPORT_SOF         0xAAu

/// Maximum analog channels supported in one frame.
#define COMBUS_TRANSPORT_MAX_ANALOG  16u

/// Maximum digital channels supported in one frame (2 packed bytes = 16 bits).
#define COMBUS_TRANSPORT_MAX_DIGITAL 16u

/// Maximum frame size in bytes (header + 2 digital bytes + 16×2 analog + CRC).
#define COMBUS_TRANSPORT_MAX_FRAME   42u

/// Minimum valid frame size (header only, 0 channels + CRC).
#define COMBUS_TRANSPORT_MIN_FRAME   8u


// =============================================================================
// 2. FLAGS BITMASK
// =============================================================================

/// Frame flags field — bit positions.
#define COMBUS_FLAG_BATTERY_LOW  (1u << 0)  ///< ComBus.batteryIsLow
#define COMBUS_FLAG_KEY_ON       (1u << 1)  ///< ComBus.keyOn
#define COMBUS_FLAG_FAILSAFE     (1u << 2)  ///< upstream failsafe active


// =============================================================================
// 3. STRUCTURES
// =============================================================================

/**
 * @brief Decoded ComBus snapshot — lightweight value-only representation.
 *
 * @details Used by the receiver side after decoding a transport frame.
 * The receiver maps these values back onto its local ComBus arrays.
 */
typedef struct {
    uint8_t  envId;                                        ///< Machine type ID (matches sender's MACHINE_TYPE)
    uint8_t  seq;                                          ///< Sequence number
    uint8_t  runLevel;                                     ///< RunLevel as uint8_t
    uint8_t  flags;                                        ///< COMBUS_FLAG_* bits
    uint8_t  nAnalog;                                      ///< Number of analog values present
    uint8_t  nDigital;                                     ///< Number of digital values present
    uint16_t analog[COMBUS_TRANSPORT_MAX_ANALOG];          ///< Analog channel values
    bool     digital[COMBUS_TRANSPORT_MAX_DIGITAL];        ///< Digital channel values (unpacked)
} ComBusSnapshot;


// =============================================================================
// 4. PUBLIC API
// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode a ComBus state into a binary transport frame.
 *
 * @details Serializes runLevel, flags, all analog and digital channels into
 * the compact frame format. CRC8 is appended automatically.
 *
 * @param[out] buf        Output buffer — must be ≥ COMBUS_TRANSPORT_MAX_FRAME bytes.
 * @param[in]  bus        Source ComBus to encode.
 * @param[in]  nAnalog    Number of analog bus channels to include.
 * @param[in]  nDigital   Number of digital bus channels to include.
 * @param[in]  envId      Machine type identifier (MACHINE_TYPE build value).
 * @param[in]  seq        Sequence counter (caller increments).
 * @param[in]  failSafe   Upstream failsafe flag.
 * @return Number of bytes written into buf, 0 on error.
 */
uint8_t combus_transport_encode(uint8_t*       buf,
                                 const ComBus*  bus,
                                 uint8_t        nAnalog,
                                 uint8_t        nDigital,
                                 uint8_t        envId,
                                 uint8_t        seq,
                                 bool           failSafe);

/**
 * @brief Decode a binary transport frame into a ComBusSnapshot.
 *
 * @details Validates SOF, frame length, and CRC before unpacking.
 *
 * @param[out] snap    Output snapshot — populated on success.
 * @param[in]  buf     Input buffer.
 * @param[in]  len     Number of bytes available in buf.
 * @return true if frame is valid and snap was populated, false otherwise.
 */
bool combus_transport_decode(ComBusSnapshot* snap,
                              const uint8_t*  buf,
                              uint8_t         len);

/**
 * @brief Apply a decoded snapshot onto a live ComBus.
 *
 * @details Writes runLevel, flags, analog and digital values into the
 * provided ComBus arrays. Caller must ensure array sizes match nAnalog
 * and nDigital in the snapshot.
 *
 * @param[out] bus     Target ComBus to update.
 * @param[in]  snap    Source snapshot (from combus_transport_decode).
 * @param[in]  nAnalog Maximum analog channels to update (safety clamp).
 * @param[in]  nDig    Maximum digital channels to update (safety clamp).
 */
void combus_transport_apply(ComBus*               bus,
                             const ComBusSnapshot* snap,
                             uint8_t               nAnalog,
                             uint8_t               nDig);

/**
 * @brief Compute CRC-8/MAXIM over a byte buffer.
 *
 * @param[in] data  Input bytes.
 * @param[in] len   Number of bytes.
 * @return CRC8 value.
 */
uint8_t combus_transport_crc8(const uint8_t* data, uint8_t len);

#ifdef __cplusplus
}
#endif

// EOF combus_transport.h
