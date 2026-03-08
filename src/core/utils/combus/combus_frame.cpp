/******************************************************************************
 * @file combus_frame.cpp
 * ComBus binary frame codec — encoding, decoding, CRC-8, and bus application.
 *
 * @details Implements frame encoding, decoding, CRC-8/MAXIM and
 * frame-to-ComBus application. The algorithm is platform-independent
 * (no Arduino or ESP-IDF calls) so it compiles on both ESP32 targets.
 *****************************************************************************/

#include "combus_frame.h"

#include <string.h>


// =============================================================================
// 1. CRC
// =============================================================================

/**
 * CRC-8/MAXIM (Dallas 1-Wire) — polynomial 0x31, init 0x00, reflect in/out.
 *
 * @details Iterative byte-by-byte computation — no lookup table needed at
 * the frame sizes used here. Safe to call from any context.
 */
uint8_t combus_frame_crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0x00u;
    for (uint8_t i = 0; i < len; ++i) {
        uint8_t byte = data[i];
        for (uint8_t b = 0; b < 8u; ++b) {
            uint8_t mix = (crc ^ byte) & 0x01u;
            crc >>= 1u;
            if (mix) {
                crc ^= 0x8Cu;
            }
            byte >>= 1u;
        }
    }
    return crc;
}


// =============================================================================
// 2. ENCODE
// =============================================================================

/**
 * Encode a ComBus state into a binary frame.
 *
 * @details Frame layout — see combus_frame.h for full spec.
 *
 * @return Number of bytes written, 0 on parameter error or uint8_t overflow.
 */
uint8_t combus_frame_encode(uint8_t*       buf,
                             const ComBus*  bus,
                             uint8_t        nAnalog,
                             uint8_t        nDigital,
                             uint8_t        envId,
                             uint8_t        seq,
                             bool           failSafe) {
      // --- 1. Guard conditions ---
    if (!buf || !bus) {
        return 0u;
    }
    uint8_t nDigBytes = (nDigital + 7u) / 8u;   // ceil(nDigital / 8)

      // Protocol invariant: the frame length field is uint8_t — reject if it would overflow.
      // Physical transport caps (CombusPhysUartMax, CombusPhysEspNowMax) are enforced
      // at the call site via static_assert in the relevant output config header.
    if ((7u + (uint16_t)nDigBytes + (uint16_t)nAnalog * 2u + 1u) > 255u) {
        return 0u;
    }

      // --- 2. Build flags byte (transport status only) ---
    // batteryIsLow and keyOn are transmitted as regular digital channels.
    uint8_t flags = 0u;
    if (failSafe) { flags |= COMBUS_FLAG_FAILSAFE; }

      // --- 3. Write fixed header ---
    uint8_t idx = 0u;
    buf[idx++] = COMBUS_FRAME_SOF;
    buf[idx++] = envId;
    buf[idx++] = seq;
    buf[idx++] = (uint8_t)bus->runLevel;
    buf[idx++] = flags;
    buf[idx++] = nAnalog;
    buf[idx++] = nDigBytes;

      // --- 4. Pack digital bits (LSB = channel 0) ---
    for (uint8_t b = 0u; b < nDigBytes; ++b) {
        uint8_t packed = 0u;
        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            uint8_t ch = (uint8_t)(b * 8u + bit);
            if (ch < nDigital && bus->digitalBus && bus->digitalBus[ch].value) {
                packed |= (uint8_t)(1u << bit);
            }
        }
        buf[idx++] = packed;
    }

      // --- 5. Write analog values (uint16_t little-endian) ---
    for (uint8_t a = 0u; a < nAnalog; ++a) {
        uint16_t val = bus->analogBus ? bus->analogBus[a].value : 0u;
        buf[idx++] = (uint8_t)(val & 0xFFu);
        buf[idx++] = (uint8_t)((val >> 8u) & 0xFFu);
    }

      // --- 6. Append CRC8 ---
    buf[idx] = combus_frame_crc8(buf, idx);
    idx++;

    return idx;
}


// =============================================================================
// 3. DECODE
// =============================================================================

/**
 * Decode and validate a binary frame into a ComBusFrame.
 *
 * @return true if SOF, length, and CRC are valid.
 */
bool combus_frame_decode(ComBusFrame*    frame,
                          const uint8_t*  buf,
                          uint8_t         len,
                          uint8_t         maxAnalog,
                          uint8_t         maxDigital) {
      // --- 1. Minimum length and SOF guard ---
    if (!frame || !buf || len < COMBUS_FRAME_MIN_LEN) {
        return false;
    }
    if (buf[0] != COMBUS_FRAME_SOF) {
        return false;
    }

      // --- 2. Parse header fields ---
    uint8_t envId     = buf[1];
    uint8_t seq       = buf[2];
    uint8_t runLevel  = buf[3];
    uint8_t flags     = buf[4];
    uint8_t nAnalog   = buf[5];
    uint8_t nDigBytes = buf[6];

      // --- 3. Sanity-check declared sizes ---
    if (!frame->analog || !frame->digital)  { return false; }

      // Protocol invariant: reject if computed frame length overflows uint8_t.
    uint16_t expectedLenW = 7u + (uint16_t)nDigBytes + (uint16_t)nAnalog * 2u + 1u;
    if (expectedLenW > 255u)                { return false; }

      // Reject if analog payload would overflow caller's buffer.
    if (nAnalog > maxAnalog)                { return false; }
      // Digital excess bits are silently truncated in the unpack loop below.

    uint8_t expectedLen = (uint8_t)expectedLenW;
    if (len < expectedLen)                  { return false; }

      // --- 4. Validate CRC ---
    uint8_t crcExpected = buf[expectedLen - 1u];
    uint8_t crcActual   = combus_frame_crc8(buf, (uint8_t)(expectedLen - 1u));
    if (crcActual != crcExpected)           { return false; }

      // --- 5. Unpack digital bits ---
    uint8_t nDigital = (uint8_t)(nDigBytes * 8u);
    if (nDigital > maxDigital) { nDigital = maxDigital; }  // clamp to caller's buffer

    uint8_t idx = 7u;
    for (uint8_t b = 0u; b < nDigBytes; ++b) {
        uint8_t packed = buf[idx++];
        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            uint8_t ch = (uint8_t)(b * 8u + bit);
            if (ch < maxDigital) {
                frame->digital[ch] = (packed >> bit) & 0x01u;
            }
        }
    }

      // --- 6. Unpack analog values (uint16_t LE) ---
    for (uint8_t a = 0u; a < nAnalog; ++a) {
        uint16_t lo  = buf[idx++];
        uint16_t hi  = buf[idx++];
        frame->analog[a] = (uint16_t)(lo | (hi << 8u));
    }

      // --- 7. Populate frame header ---
    frame->envId    = envId;
    frame->seq      = seq;
    frame->runLevel = runLevel;
    frame->flags    = flags;
    frame->nAnalog  = nAnalog;
    frame->nDigital = nDigital;

    return true;
}


// =============================================================================
// 4. APPLY FRAME → COMBUS
// =============================================================================

/**
 * Apply a decoded frame onto a live ComBus.
 *
 * @details Updates runLevel, flags, and all channel values.
 * Caller ensures array bounds match nAnalog & nDig.
 */
void combus_frame_apply(ComBus*            bus,
                         const ComBusFrame* frame,
                         uint8_t            nAnalog,
                         uint8_t            nDig) {
    if (!bus || !frame) {
        return;
    }

      // --- RunLevel ---
    bus->runLevel = (RunLevel)frame->runLevel;

      // --- Flags (transport status only) ---
    // batteryIsLow and keyOn come from digitalBus[] via the digital loop below.

      // --- Analog channels ---
    uint8_t aN = (frame->nAnalog < nAnalog) ? frame->nAnalog : nAnalog;
    if (bus->analogBus) {
        for (uint8_t i = 0u; i < aN; ++i) {
            bus->analogBus[i].value    = frame->analog[i];
            bus->analogBus[i].isDrived = true;
        }
    }

      // --- Digital channels ---
    uint8_t dN = (frame->nDigital < nDig) ? frame->nDigital : nDig;
    if (bus->digitalBus) {
        for (uint8_t i = 0u; i < dN; ++i) {
            bus->digitalBus[i].value    = frame->digital[i];
            bus->digitalBus[i].isDrived = true;
        }
    }
}

// EOF combus_frame.cpp
