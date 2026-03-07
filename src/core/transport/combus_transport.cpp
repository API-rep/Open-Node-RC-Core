/******************************************************************************
 * @file combus_transport.cpp
 * Generic ComBus binary transport — frame encoding / decoding.
 *
 * @details Implements frame encoding, decoding, CRC-8/MAXIM and
 * snapshot-to-ComBus application. The algorithm is platform-independent
 * (no Arduino or ESP-IDF calls) so it compiles on both ESP32 targets.
 *****************************************************************************/

#include "combus_transport.h"

#include <string.h>


// =============================================================================
// 1. PRIVATE HELPERS
// =============================================================================

/**
 * CRC-8/MAXIM (Dallas 1-Wire) — polynomial 0x31, init 0x00, reflect in/out.
 *
 * @details Iterative byte-by-byte computation — no lookup table needed at
 * the frame sizes used here (≤ 42 bytes). Safe to call from any context.
 */
uint8_t combus_transport_crc8(const uint8_t* data, uint8_t len) {
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
 * Encode a ComBus state into a binary transport frame.
 *
 * @details Frame layout — see combus_transport.h for full spec.
 *
 * @return Number of bytes written, 0 on parameter error.
 */
uint8_t combus_transport_encode(uint8_t*       buf,
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
    if (nAnalog  > COMBUS_TRANSPORT_MAX_ANALOG)  { nAnalog  = COMBUS_TRANSPORT_MAX_ANALOG;  }
    if (nDigital > COMBUS_TRANSPORT_MAX_DIGITAL) { nDigital = COMBUS_TRANSPORT_MAX_DIGITAL; }

    uint8_t nDigBytes = (nDigital + 7u) / 8u;  // ceil(nDigital / 8)

      // --- 2. Build flags byte ---
    uint8_t flags = 0u;
    if (bus->batteryIsLow) { flags |= COMBUS_FLAG_BATTERY_LOW; }
    if (bus->keyOn)        { flags |= COMBUS_FLAG_KEY_ON;      }
    if (failSafe)          { flags |= COMBUS_FLAG_FAILSAFE;    }

      // --- 3. Write fixed header ---
    uint8_t idx = 0u;
    buf[idx++] = COMBUS_TRANSPORT_SOF;
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
    buf[idx] = combus_transport_crc8(buf, idx);
    idx++;

    return idx;
}


// =============================================================================
// 3. DECODE
// =============================================================================

/**
 * Decode and validate a binary transport frame into a ComBusSnapshot.
 *
 * @return true if SOF, length, and CRC are valid.
 */
bool combus_transport_decode(ComBusSnapshot* snap,
                              const uint8_t*  buf,
                              uint8_t         len) {
      // --- 1. Minimum length and SOF guard ---
    if (!snap || !buf || len < COMBUS_TRANSPORT_MIN_FRAME) {
        return false;
    }
    if (buf[0] != COMBUS_TRANSPORT_SOF) {
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
    if (nAnalog   > COMBUS_TRANSPORT_MAX_ANALOG)        { return false; }
    if (nDigBytes > (COMBUS_TRANSPORT_MAX_DIGITAL / 8u)) { return false; }

    uint8_t expectedLen = 7u + nDigBytes + (uint8_t)(nAnalog * 2u) + 1u;
    if (len < expectedLen) {
        return false;
    }

      // --- 4. Validate CRC ---
    uint8_t crcExpected = buf[expectedLen - 1u];
    uint8_t crcActual   = combus_transport_crc8(buf, (uint8_t)(expectedLen - 1u));
    if (crcActual != crcExpected) {
        return false;
    }

      // --- 5. Unpack digital bits ---
    uint8_t nDigital = (uint8_t)(nDigBytes * 8u);
    if (nDigital > COMBUS_TRANSPORT_MAX_DIGITAL) { nDigital = COMBUS_TRANSPORT_MAX_DIGITAL; }

    uint8_t idx = 7u;
    for (uint8_t b = 0u; b < nDigBytes; ++b) {
        uint8_t packed = buf[idx++];
        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            uint8_t ch = (uint8_t)(b * 8u + bit);
            if (ch < COMBUS_TRANSPORT_MAX_DIGITAL) {
                snap->digital[ch] = (packed >> bit) & 0x01u;
            }
        }
    }

      // --- 6. Unpack analog values (uint16_t LE) ---
    for (uint8_t a = 0u; a < nAnalog; ++a) {
        uint16_t lo  = buf[idx++];
        uint16_t hi  = buf[idx++];
        snap->analog[a] = (uint16_t)(lo | (hi << 8u));
    }

      // --- 7. Populate snapshot header ---
    snap->envId    = envId;
    snap->seq      = seq;
    snap->runLevel = runLevel;
    snap->flags    = flags;
    snap->nAnalog  = nAnalog;
    snap->nDigital = nDigital;

    return true;
}


// =============================================================================
// 4. APPLY SNAPSHOT → COMBUS
// =============================================================================

/**
 * Apply a decoded snapshot onto a live ComBus.
 *
 * @details Updates runLevel, flags, and all channel values.
 * Caller ensures array bounds match nAnalog & nDig.
 */
void combus_transport_apply(ComBus*               bus,
                             const ComBusSnapshot* snap,
                             uint8_t               nAnalog,
                             uint8_t               nDig) {
    if (!bus || !snap) {
        return;
    }

      // --- RunLevel ---
    bus->runLevel = (RunLevel)snap->runLevel;

      // --- Flags ---
    bus->batteryIsLow = (snap->flags & COMBUS_FLAG_BATTERY_LOW) != 0u;
    bus->keyOn        = (snap->flags & COMBUS_FLAG_KEY_ON)      != 0u;

      // --- Analog channels ---
    uint8_t aN = (snap->nAnalog < nAnalog) ? snap->nAnalog : nAnalog;
    if (bus->analogBus) {
        for (uint8_t i = 0u; i < aN; ++i) {
            bus->analogBus[i].value    = snap->analog[i];
            bus->analogBus[i].isDrived = true;
        }
    }

      // --- Digital channels ---
    uint8_t dN = (snap->nDigital < nDig) ? snap->nDigital : nDig;
    if (bus->digitalBus) {
        for (uint8_t i = 0u; i < dN; ++i) {
            bus->digitalBus[i].value    = snap->digital[i];
            bus->digitalBus[i].isDrived = true;
        }
    }
}

// EOF combus_transport.cpp
