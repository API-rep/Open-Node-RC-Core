/******************************************************************************
 * @file combus_frame.h
 * ComBus binary frame codec — encoding, decoding, CRC-8, and bus application.
 *
 * @details Pure frame codec for the ComBus binary transport protocol.
 * Contains only the frame format definition and the encode / decode / apply
 * functions. Has no knowledge of any physical transport medium (UART,
 * ESP-Now…). Transport-specific constraints (buffer sizes, baud rates,
 * payload caps) belong in the relevant output config files:
 *   src/core/config/outputs/sound_uart.h    — UART transport
 *   src/core/config/outputs/combus_espnow.h — ESP-Now transport (future)
 *
 * Frame layout (variable length):
 * @code
 *  Offset  Size  Field
 *  ------  ----  -----
 *   0       1    SOF          0xAA (start-of-frame)
 *   1       1    env_id       MACHINE_TYPE numeric value (identifies ComBus layout)
 *   2       1    seq          frame rolling counter 0-255
 *   3       1    run_level    RunLevel cast to uint8_t
 *   4       1    flags        bit0=failSafe
 *   5       1    n_analog     number of analog channels
 *   6       1    n_dig_bytes  ceil(n_digital / 8)
 *   7       var  digital[]    digital bits packed LSB-first
 *   7+d     var  analog[]     uint16_t LE × n_analog
 *   last    1    crc8         CRC-8/MAXIM over bytes [0 … last-1]
 * @endcode
 *
 * @note analogBusMaxVal is a static build-time constant — not transmitted.
 *       The receiver must share the same MACHINE_TYPE to interpret values.
 *
 * @note [ESP-NOW — FUTURE] Instance filtering:
 *       `env_id` identifies the ComBus *layout type* (e.g. DUMPER_TRUCK = 1),
 *       not the individual machine instance. Two machines of the same type
 *       would both emit env_id = 1 on the same ESP-Now channel.
 *       Instance filtering MUST be done at the ESP-Now layer using the
 *       sender's MAC address: pair each receiver to its specific machine MAC
 *       at init, and reject frames whose source MAC does not match.
 *       Do NOT rely on env_id alone for instance discrimination.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <struct/combus_struct.h>
#include <struct/outputs_struct.h>


// =============================================================================
// 1. CONSTANTS
// =============================================================================

/// Start-of-frame sentinel byte.
#define COMBUS_FRAME_SOF         0xAAu

/// Minimum valid frame size (7-byte header + CRC, zero channels).
#define COMBUS_FRAME_MIN_LEN     8u


// =============================================================================
// 2. FLAGS BITMASK
// =============================================================================

/// Frame flags field — bit positions.
/// Only transport-level status lives here; application state (batteryIsLow,
/// keyOn) is carried as normal digital channels.
#define COMBUS_FLAG_FAILSAFE     (1u << 0)  ///< upstream failsafe active (link-loss or remote disconnect)


// =============================================================================
// 3. PUBLIC API
// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encode a ComBus state into a binary frame.
 *
 * @details Serializes runLevel, flags, all analog and digital channels into
 * the compact frame format. CRC8 is appended automatically.
 * Returns 0 if buf/bus are null or if the computed frame length would
 * overflow a uint8_t (i.e. caller requested more data than the protocol
 * can address with a single-byte length field).
 *
 * @param[out] buf        Output buffer — must be sized by the caller to fit
 *                        the expected frame (use SoundTransportFrameSize or
 *                        the equivalent for the active transport).
 * @param[in]  bus        Source ComBus to encode.
 * @param[in]  nAnalog    Number of analog bus channels to include.
 * @param[in]  nDigital   Number of digital bus channels to include.
 * @param[in]  envId      Machine type identifier (MACHINE_TYPE build value).
 * @param[in]  seq        Sequence counter (caller increments).
 * @param[in]  failSafe   Upstream failsafe flag.
 * @return Number of bytes written into buf, 0 on error.
 */
uint8_t combus_frame_encode(uint8_t*       buf,
                             const ComBus*  bus,
                             uint8_t        nAnalog,
                             uint8_t        nDigital,
                             uint8_t        envId,
                             uint8_t        seq,
                             bool           failSafe);

/**
 * @brief Decode a binary frame into a ComBusFrame.
 *
 * @details Validates SOF, frame length, and CRC before unpacking.
 * Rejects frames whose analog count exceeds maxAnalog.
 * Digital bits exceeding maxDigital are silently truncated.
 *
 * @param[out] frame      Output frame — populated on success.
 * @param[in]  buf        Input buffer.
 * @param[in]  len        Number of bytes available in buf.
 * @param[in]  maxAnalog  Capacity of frame->analog[]  (caller's buffer size).
 * @param[in]  maxDigital Capacity of frame->digital[] (caller's buffer size).
 * @return true if frame is valid and was populated, false otherwise.
 */
bool combus_frame_decode(ComBusFrame*    frame,
                          const uint8_t*  buf,
                          uint8_t         len,
                          uint8_t         maxAnalog,
                          uint8_t         maxDigital);

/**
 * @brief Apply a decoded frame onto a live ComBus.
 *
 * @details Writes runLevel, flags, analog and digital values into the
 * provided ComBus arrays. Caller must ensure array sizes match nAnalog
 * and nDigital in the frame.
 *
 * @param[out] bus     Target ComBus to update.
 * @param[in]  frame   Source frame (from combus_frame_decode).
 * @param[in]  nAnalog Maximum analog channels to update (safety clamp).
 * @param[in]  nDig    Maximum digital channels to update (safety clamp).
 */
void combus_frame_apply(ComBus*            bus,
                         const ComBusFrame* frame,
                         uint8_t            nAnalog,
                         uint8_t            nDig);

/**
 * @brief Compute CRC-8/MAXIM over a byte buffer.
 *
 * @param[in] data  Input bytes.
 * @param[in] len   Number of bytes.
 * @return CRC8 value.
 */
uint8_t combus_frame_crc8(const uint8_t* data, uint8_t len);

#ifdef __cplusplus
}
#endif

// EOF combus_frame.h
