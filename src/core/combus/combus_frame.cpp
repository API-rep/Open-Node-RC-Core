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
#include <Arduino.h>


// =============================================================================
// 1. CRC
// =============================================================================

/**
 * CRC-8/MAXIM (Dallas 1-Wire) — polynomial 0x31, init 0x00, reflect in/out.
 *
 * @details Iterative byte-by-byte computation — no lookup table needed at
 * the frame sizes used here. Safe to call from any context.
 *
 * @param[in] data  Input byte buffer.
 * @param[in] len   Number of bytes to process.
 * 
 * @return CRC-8 value over the input buffer.
 */

uint8_t combus_frame_crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0x00;
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
 * Encode a ComBus state into a binary frame written into outputBuffer.
 *
 * @details Serialization sequence:
 *   1. Null pointer and frame size overflow guard (returns 0 on failure).
 *   2. Build flags byte from transport-level inputs (failSafe, ...).
 *   3. Write fixed 7-byte header: SOF, envId, nAnalog, nDigital, seq, runLevel, flags.
 *   4. Pack digital channel values as bits, LSB-first, ceil(nDigital/8) bytes.
 *   5. Write analog channel values as uint16_t little-endian, nAnalog entries.
 *   6. Append CRC-8/MAXIM over all preceding bytes.
 *
 * The caller is responsible for sizing outputBuffer to at least
 * CombusFrameHeaderLen + ceil(nDigital/8) + nAnalog*2 + 1 bytes.
 *
 * @param[out] outputBuffer  Destination buffer (sized by caller).
 * @param[in]  combus        Source ComBus instance to encode.
 * @param[in]  nAnalog       Number of analog channels to include.
 * @param[in]  nDigital      Number of digital channels to include.
 * @param[in]  envId         Machine type identifier (MACHINE_TYPE build value).
 * @param[in]  seq           Rolling sequence counter (caller increments).
 * @param[in]  failSafe      Upstream failsafe flag (sets COMBUS_FLAG_FAILSAFE).
 *
 * @return Number of bytes written into outputBuffer, 0 on error.
 */

uint8_t combus_frame_encode( const ComBusFrameCfg& cfg,
                             uint8_t*              outputBuffer,
                             const ComBus*         combus,
                             uint8_t               seq,
                             bool                  failSafe ) {

    const uint8_t nAnalog  = cfg.nAnalog;
    const uint8_t nDigital = cfg.nDigital;
    const uint8_t envId    = cfg.envId;
                               
      // --- 1. Guard conditions — null pointer + frame size overflow ---
    if (!outputBuffer || !combus) {
        return 0;
    }
      // Reject frame size over max uint8_t size (255u) to avoid overflow
    uint8_t nDigBytes = (nDigital + 7u) / 8u;   // ceil(nDigital / 8)

    if ((CombusFrameHeaderLen + (uint16_t)nDigBytes + (uint16_t)nAnalog * 2u + 1u) > 255u) {
        return 0;
    }

      // --- 2. Build flags byte ---
    uint8_t flags = 0;   // transport-level status bits (COMBUS_FLAG_*)

    if (failSafe) { flags |= COMBUS_FLAG_FAILSAFE; }
    //if (...)     { flags |= COMBUS_FLAG_... ; }       // next flag — bit 1
    //if (...)     { flags |= COMBUS_FLAG_... ; }       // next flag — bit 2

      // --- 3. Write fixed header ---
    uint8_t pos = 0;   // write position in outputBuffer
    
    outputBuffer[pos++] = CombusFrameSof;
    outputBuffer[pos++] = envId;
    outputBuffer[pos++] = nAnalog;
    outputBuffer[pos++] = nDigital;
    outputBuffer[pos++] = seq;
    outputBuffer[pos++] = (uint8_t)combus->runLevel;
    outputBuffer[pos++] = flags;

      // --- 4. Pack digital bits values into bytes (bitbool lsb mode) ---
    for (uint8_t b = 0; b < nDigBytes; ++b) {
        uint8_t packed = 0;
      
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            uint8_t ch = (uint8_t)(b * 8u + bit);

            if (ch < nDigital && combus->digitalBus && combus->digitalBus[ch].value) {
                packed |= (uint8_t)(1u << bit);
            }
        }

        outputBuffer[pos++] = packed;
    }

      // --- 5. Write analog values (uint16_t little-endian) ---
    for (uint8_t a = 0; a < nAnalog; ++a) {
        uint16_t val = combus->analogBus ? combus->analogBus[a].value : 0;
        outputBuffer[pos++] = (uint8_t)(val & 0xFFu);
        outputBuffer[pos++] = (uint8_t)((val >> 8u) & 0xFFu);
    }

      // --- 6. Append CRC8 ---
    outputBuffer[pos] = combus_frame_crc8(outputBuffer, pos);
    pos++;

    return pos;
}



// =============================================================================
// 3. DECODE
// =============================================================================

/**
 * Decode and validate a binary frame from inputBuffer into outputFrame.
 *
 * @details Validation sequence (any failure returns false):
 *   1. Null pointer and minimum length check (>= CombusFrameMinLen).
 *   2. SOF sentinel check (inputBuffer[0] == CombusFrameSof).
 *   3. Output buffer pointer check (outputFrame->analog and ->digital must be set by caller).
 *   4. Declared payload size sanity (computed frame length must fit in uint8_t).
 *   5. Analog overflow check (header.cfg.nAnalog <= maxAnalog).
 *   6. Received length check (len >= computed expected length).
 *   7. CRC-8/MAXIM check over bytes [0 ... expectedLen-2].
 *
 * On success, unpacks digital bits (LSB-first) and analog uint16_t LE values
 * into the caller-provided arrays, then copies the header into outputFrame.
 *
 * Digital channels exceeding digitalBufSize are silently dropped (not an error).
 * outputFrame->header.cfg.nDigital reflects the clamped count after decoding.
 *
 * @param[out] outputFrame     Destination frame — analog/digital pointers must be
 *                              pre-set by the caller to buffers of at least
 *                              analogBufSize / digitalBufSize entries.
 * @param[in]  inputBuffer     Raw received bytes.
 * @param[in]  len             Number of valid bytes in inputBuffer.
 * @param[in]  analogBufSize   Capacity of outputFrame->analog[]  (caller's buffer size).
 * @param[in]  digitalBufSize  Capacity of outputFrame->digital[] (caller's buffer size).
 *
 * @return true if frame is valid and outputFrame was fully populated, false otherwise.
 */

bool combus_frame_decode( const ComBusFrameCfg& cfg,
                          ComBusFrame*          outputFrame,
                          const uint8_t*        inputBuffer,
                          uint8_t               len ) {

    const uint8_t analogBufSize  = cfg.nAnalog;
    const uint8_t digitalBufSize = cfg.nDigital;

      // --- 1. Minimum length and SOF guard check ---
    if (!outputFrame || !inputBuffer || len < CombusFrameMinLen) {return false;}
    if (inputBuffer[0] != CombusFrameSof) {return false;}

      // --- 2. Parse header fields ---
    CombusFrameHeader header;

    memcpy(&header, inputBuffer + 1u, sizeof(header));   // skip SOF byte at offset 0

    uint8_t nDigBytes = (header.cfg.nDigital + 7u) / 8u;   // derive packed byte count locally

      // --- 3. Sanity-check declared sizes ---
    if (!outputFrame->analog || !outputFrame->digital)  { return false; }

      // Reject if computed frame length overflows uint8_t.
    uint16_t expectedLenW = CombusFrameHeaderLen + (uint16_t)nDigBytes + (uint16_t)header.cfg.nAnalog * 2u + 1u;
    if (expectedLenW > 255u)                { return false; }

      // Reject if analog payload would overflow caller's buffer.
    if (header.cfg.nAnalog > analogBufSize)     { return false; }
      // Digital excess bits are silently truncated in the unpack loop below.

    uint8_t expectedLen = (uint8_t)expectedLenW;
    if (len < expectedLen)                  { return false; }

      // --- 4. Validate CRC ---
    uint8_t crcExpected = inputBuffer[expectedLen - 1u];
    uint8_t crcActual   = combus_frame_crc8(inputBuffer, (uint8_t)(expectedLen - 1u));
    if (crcActual != crcExpected)           { return false; }

      // --- 5. Unpack digital bits ---
    uint8_t nDigital = header.cfg.nDigital;

    if (nDigital > digitalBufSize) { nDigital = digitalBufSize; }  // clamp to caller's buffer

    uint8_t pos = CombusFrameHeaderLen;
    for (uint8_t b = 0; b < nDigBytes; ++b) {
        uint8_t packed = inputBuffer[pos++];
        for (uint8_t bit = 0; bit < 8u; ++bit) {
            uint8_t ch = (uint8_t)(b * 8u + bit);
            if (ch < digitalBufSize) {
                outputFrame->digital[ch] = (packed >> bit) & 0x01u;
            }
        }
    }

      // --- 6. Unpack analog values (uint16_t LE) ---
    for (uint8_t a = 0; a < header.cfg.nAnalog; ++a) {
        uint16_t lo  = inputBuffer[pos++];
        uint16_t hi  = inputBuffer[pos++];
        outputFrame->analog[a] = (uint16_t)(lo | (hi << 8u));
    }

      // --- 7. Populate frame header ---
    outputFrame->header              = header;
    outputFrame->header.cfg.nDigital = nDigital;   // apply clamped value (may differ from wire value)

    return true;
}


// =============================================================================
// 4. APPLY FRAME → COMBUS
// =============================================================================

/**
 * Apply a decoded ComBusFrame onto a live ComBus instance.
 *
 * @details Application sequence:
 *   1. Null pointer guard (returns on failure).
 *   2. Write runLevel from frame header; stamp combus->lastFrameMs = millis()
 *      so combus_watchdog can detect frame loss and clear isDrived.
 *   3. Reserved — transport flags (unused for now).
 *   4. Write analog channels [0 .. min(cfg.nAnalog, header.cfg.nAnalog)-1]
 *      and mark isDrived = true.
 *   5. Write digital channels [0 .. min(cfg.nDigital, header.cfg.nDigital)-1]
 *      and mark isDrived = true.
 *
 * Channels beyond the effective count are left untouched in the live bus.
 * Caller must ensure combus->analogBus and combus->digitalBus arrays are
 * allocated and sized to at least cfg.nAnalog / cfg.nDigital entries.
 *
 * @param[out] combus      Target ComBus instance to update.
 * @param[in]  inputFrame  Populated frame from combus_frame_decode().
 * @param[in]  cfg         Layout descriptor used as upper clamp (nAnalog, nDigital).
 */
void combus_frame_apply( const ComBusFrameCfg& cfg,
                         ComBus*               combus,
                         const ComBusFrame*    inputFrame ) {

      //  Analog and digital channels upper clamp
    const uint8_t nAnalog  = cfg.nAnalog;    // analog channels bus capacity
    const uint8_t nDigital = cfg.nDigital;   // digital channels bus capacity

      // --- 1. Guard conditions — null pointer ---
    if (!combus || !inputFrame) {
        return;
    }

      // --- 2. RunLevel + watchdog timestamp ---
    combus->runLevel    = (RunLevel)inputFrame->header.runLevel;
    combus->lastFrameMs = millis();   // combus_watchdog uses this to detect frame loss and clear isDrived

      // --- 3. Flags (transport status only) ---


      // --- 4. Analog channels ---
    uint8_t nAnalogEff = (inputFrame->header.cfg.nAnalog < nAnalog) ? inputFrame->header.cfg.nAnalog : nAnalog;

    if (combus->analogBus) {
        for (uint8_t i = 0; i < nAnalogEff; ++i) {
            combus->analogBus[i].value    = inputFrame->analog[i];
            combus->analogBus[i].isDrived = true;
        }
    }

      // --- 5. Digital channels ---
    uint8_t nDigitalEff = (inputFrame->header.cfg.nDigital < nDigital) ? inputFrame->header.cfg.nDigital : nDigital;

    if (combus->digitalBus) {
        for (uint8_t i = 0; i < nDigitalEff; ++i) {
            combus->digitalBus[i].value    = inputFrame->digital[i];
            combus->digitalBus[i].isDrived = true;
        }
    }
}

// EOF combus_frame.cpp
