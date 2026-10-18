/******************************************************************************
 * @file test_combus_loopback.cpp
 * @brief ComBus codec + UART loopback test suite.
 *
 * @details Five independent test groups:
 *
 *   Group A — codec round-trip (no hardware required):
 *     Fills the generated machine ComBus with random analog/digital data,
 *     encodes it into a binary frame via combus_frame_encode(), then
 *     decodes the result via combus_frame_decode() and asserts field-by-field
 *     equality between the ComBus and the decoded ComBusFrame.  Also checks
 *     that seq == 0 (handshake) is refused and that the flags byte is 0.
 *
 *   Group B — UART loopback (requires TX↔RX wire on Serial2):
 *     Uses combus_tx_update() and combus_rx_update() end-to-end.
 *     Sends N frames with random payloads, then polls combus_rx_snapshot()
 *     and asserts field equality. Also verifies SOF-scan re-sync by injecting
 *     garbage bytes before a valid frame.
 *
 *   Group C — view identity (Phase 3 / A13):
 *     Validates that the 3 view-specific enums (AnalogComBusID,
 *     AnalogComBusSystemID, AnalogComBusBaseID and their digital
 *     counterparts) reference the SAME runtime object in
 *     AnalogComBusArray[] / DigitalComBusArray[].
 *
 *   Group D — wire-end sizing (LY2):
 *     Verifies that combus_frame_encode() sizes the wire payload to the
 *     layer's prefix length (BASE shorter than FULL), with expected sizes
 *     computed from the generated CH_COUNT constants.
 *
 *   Group G — RUNLEVEL round-trip (RL3).
 *
 *   Group E — MD5 pointer resolution by layer (LY4):
 *     Verifies that combus_protocol_init() resolves the per-link
 *     expectedMd5 pointer from link->layer (BASE / SYSTEM / BOARD),
 *     and that combus_handshake_compareAndLog() matches correctly when
 *     given the right local MD5 pointer.  Same pattern as Group D —
 *     verify the resolution at init, not the per-frame behaviour.
 *
 *   Group F — REMOVED (chantier 12.6 cleanup):
 *     The historical "failsafe trigger on combus link loss" test relied on
 *     the FS1 open-drain `isNotDrived` flag and a dedicated FS1 contributor
 *     channel.  Both have been removed in chantier 12.5/12.6: link health
 *     is now monitored at transport level by per-backend *_LINK_LOST
 *     contributors (PS4_DS4_BT_LINK_LOST, UART_LINK_LOST, ...) and
 *     aggregated by `remote_link_fallback_chain.cpp` into REMOTE_LINK_LOST,
 *     which feeds the top-level FAILSAFE aggregator.
 *     See failsafe_module.md §12.5 (new design) and §12.6 (cleanup).
 *
 * @note Runs on the ESP32 board. Group B requires a physical TX↔RX jumper
 *   on the Serial2 pins (GPIO 17 ↔ 16). Run via: pio test -e test_combus_loopback
 *****************************************************************************/

#include <Arduino.h>
#include <unity.h>

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/hw/transport/uart_com.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/combus/combus_defs.h>
#include <core/system/combus/protocol/frame/combus_frame_defs.h>
#include <core/system/combus/combus_access.h>          // combus_set_digital() — generic accessor
#include <core/system/combus/protocol/combus_protocol.h> // ComBusLink, combus_protocol_init() — Group E
#include <core/system/combus/protocol/frame/combus_handshake.h> // combus_handshake_compareAndLog() — Group E
#include <core/system/combus/protocol/frame/combus_handshake_rx.h> // combus_handshake_compareAndLog() — Group E (RX-side declaration)

// Phase 3 / A13: include combus.h (declares AnalogComBusArray /
// DigitalComBusArray) AND the 3 view-specific ids headers so the
// runtime identity test can reference AnalogComBusID, AnalogComBusSystemID
// and AnalogComBusBaseID in the same translation unit.
//
// combus.h transitively includes combus_ids.h, so we do NOT include
// it directly here (avoids the double-definition error: combus_ids.h
// does not yet have a #pragma once guard).
#include "combus.h"
#include "combus_system_ids.h"
#include "combus_base_ids.h"


// =============================================================================
// TEST CONFIGURATION
// =============================================================================

static constexpr uint32_t kLoopbackBaud = 115200u;

static constexpr int      kTxPin        = 17;     ///< Serial2 TX — connect to kRxPin
static constexpr int      kRxPin        = 16;     ///< Serial2 RX — connect to kTxPin
static constexpr uint32_t kRxPollMs     = 50u;    ///< poll budget per frame (ms)
static constexpr uint8_t  kMonkeyPasses = 8u;     ///< number of random frames to send


// =============================================================================
// SHARED TEST FIXTURES
// =============================================================================

// The fixtures use the generated machine ComBus (AnalogComBusArray,
// DigitalComBusArray and comBus, from combus_generated/combus.cpp): their
// sizes and channel layers match the real profile, so channel IDs such as
// AnalogComBusID::RUNLEVEL always index inside the arrays.
static constexpr uint8_t kNAnalog  = static_cast<uint8_t>(AnalogComBusID::CH_COUNT);
static constexpr uint8_t kNDigital = static_cast<uint8_t>(DigitalComBusID::CH_COUNT);

static constexpr ComBusFrameCfg kCfg      = { kNAnalog, kNDigital };
static constexpr ComBusFrameCfg kSmallCfg = { 4u, 6u };   ///< clamp test only

// --- ComBus A (source / TX side) ---
static ComBus& txComBus = comBus;

// --- ComBusFrame B (decode / RX side) ---
static uint16_t rxAnalogBuf[kNAnalog]   = {};
static bool     rxDigitalBuf[kNDigital] = {};
static ComBusFrame rxFrame = { .analog = rxAnalogBuf, .digital = rxDigitalBuf };

// --- Encode output buffer ---
static uint8_t encodeBuf[255u];

// --- Per-link state pools (caller-owned, registered once at boot) ---
static UartCtx       s_uartPool[1];
static CombusTxState s_txStates[1];
static CombusRxState s_rxStates[1];
static constexpr uint8_t kLinkIdx = 0u;


// =============================================================================
// HELPERS
// =============================================================================

/** Expected wire length of a control frame: SOF + header, digital bytes, analog values, CRC. */
static constexpr uint8_t frameLen(uint8_t nAnalog, uint8_t nDigital) {
    return (uint8_t)(CombusFrameHeaderLen + (nDigital + 7u) / 8u + nAnalog * 2u + 1u);
}

/** Fill txComBus with deterministic pseudo-random values seeded by `seed`. */
static void fillRandom(uint32_t seed) {
    srand(seed);
    for (uint8_t i = 0u; i < kNAnalog; ++i) {
        txComBus.analogBus[i].value = (uint16_t)(rand() % 1001u);
    }
    for (uint8_t i = 0u; i < kNDigital; ++i) {
        txComBus.digitalBus[i].value = (rand() % 2) == 1;
    }
    // RL3: runLevel is no longer a struct field — write RUNLEVEL via the generic
    // analog accessor (RUNLEVEL is a SYSTEM channel, so a SYSTEM caller may write it).
    combus_set_analog(txComBus, AnalogComBusID::RUNLEVEL, (uint16_t)RunLevel::RUNNING, ChanLayer::SYSTEM);
}

/** Assert that the decoded frame matches txComBus, over the channels the frame carries. */
static void assertFrameMatchesBus(const ComBusFrame* frame, const ComBus* bus) {
    TEST_ASSERT_NOT_NULL(frame);
    // RL2: runLevel is no longer a header field — it travels as a standard
    // combus channel (RUNLEVEL, scope SYSTEM), checked by the channel loop below.
    // A frame carries a prefix of the bus (its wire-end), not always the full bus.
    for (uint8_t i = 0u; i < frame->header.cfg.nAnalog; ++i) {
        TEST_ASSERT_EQUAL_UINT16(bus->analogBus[i].value, frame->analog[i]);
    }
    for (uint8_t i = 0u; i < frame->header.cfg.nDigital; ++i) {
        TEST_ASSERT_EQUAL(bus->digitalBus[i].value, frame->digital[i]);
    }
}


// =============================================================================
// GROUP A — CODEC ROUND-TRIP (no hardware)
// =============================================================================

/** Single encode→decode round-trip with a fixed seed. */
static void test_codec_single_roundtrip(void) {
    fillRandom(0xDEADBEEFul);

    // LY2 — encode takes analogWireEnd / digitalWireEnd.  Use the full
    // enum set so the codec iterates over every channel.  seq starts at 1:
    // 0 is reserved for the handshake and refused by the encoder.
    uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 1u, kNAnalog, kNDigital);
    TEST_ASSERT_EQUAL_UINT8(frameLen(kNAnalog, kNDigital), len);

    bool ok = combus_frame_decode(kCfg, &rxFrame, encodeBuf, len);
    TEST_ASSERT_TRUE(ok);

    assertFrameMatchesBus(&rxFrame, &txComBus);
}

/** seq == 0 is reserved for the handshake — the encoder must refuse it. */
static void test_codec_seq_zero_refused(void) {
    fillRandom(0x0BADF00Dul);

    uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 0u, kNAnalog, kNDigital);
    TEST_ASSERT_EQUAL_UINT8(0u, len);
}

/** The header flags byte is reserved (FS2: no failsafe bit any more) — it must decode as 0. */
static void test_codec_flags_reserved(void) {
    fillRandom(0x12345678ul);

    uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 1u, kNAnalog, kNDigital);
    TEST_ASSERT_GREATER_THAN_UINT8(0u, len);

    bool ok = combus_frame_decode(kCfg, &rxFrame, encodeBuf, len);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(0u, rxFrame.header.flags);
}

/** Corrupt one payload byte — CRC must reject the frame. */
static void test_codec_crc_reject(void) {
    fillRandom(0xCAFEBABEul);

    uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 2u, kNAnalog, kNDigital);
    TEST_ASSERT_GREATER_THAN_UINT8(0u, len);

    encodeBuf[len / 2u] ^= 0xFFu;  // flip bits in the middle of the frame

    bool ok = combus_frame_decode(kCfg, &rxFrame, encodeBuf, len);
    TEST_ASSERT_FALSE(ok);  // CRC must catch the corruption
}

/** Monkey test — N random seeds, each must round-trip cleanly. */
static void test_codec_monkey_roundtrip(void) {
    for (uint8_t pass = 0u; pass < kMonkeyPasses; ++pass) {
        fillRandom((uint32_t)pass * 0x9E3779B9ul);

        uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, (uint8_t)(pass + 1u),
                                          kNAnalog, kNDigital);
        TEST_ASSERT_GREATER_THAN_UINT8(0u, len);

        bool ok = combus_frame_decode(kCfg, &rxFrame, encodeBuf, len);
        TEST_ASSERT_TRUE_MESSAGE(ok, "Monkey round-trip failed");

        assertFrameMatchesBus(&rxFrame, &txComBus);
    }
}


// =============================================================================
// GROUP B — UART LOOPBACK (requires TX↔RX jumper on Serial2)
// =============================================================================

/** Init combus_tx and combus_rx on the same Serial2 port (loopback). */
static void test_loopback_init(void) {
    // The UART port registry must be registered before any uart_com_init().
    uart_com_register_pool(s_uartPool, 1);

    NodeCom* com = uart_com_init(&Serial2, kLoopbackBaud, kTxPin, kRxPin, "test_loopback");
    TEST_ASSERT_NOT_NULL(com);

    // Register the per-link state pools (caller-owned, registered once at boot).
    combus_tx_register_pool(s_txStates, 1);
    combus_rx_register_pool(s_rxStates, 1);

    // LY2 — combus_tx_init() takes a ChanLayer parameter.  Use BASE for
    // the loopback test (matches the machine env default): the frames
    // carry the BASE prefix of the bus.
    combus_tx_init(kLinkIdx, com, kCfg, 50u, ChanLayer::BASE);   // 50 Hz TX
    combus_rx_init(kLinkIdx, com, kCfg, rxAnalogBuf, rxDigitalBuf);
}

/** Send one frame, poll RX, assert snapshot matches what was sent. */
static void test_loopback_single_frame(void) {
    fillRandom(0xABCD1234ul);

        // Force TX to fire on next call by resetting via a fresh init would be
        // heavier — instead we just wait one full period (20ms at 50Hz) + margin.
    delay(30u);
    combus_tx_update(&txComBus);
    delay(kRxPollMs);
    combus_rx_update();

    const ComBusFrame* snap = combus_rx_snapshot(kLinkIdx);
    TEST_ASSERT_NOT_NULL_MESSAGE(snap, "No snapshot after loopback — check TX↔RX jumper");
    assertFrameMatchesBus(snap, &txComBus);
}

/** Inject garbage before a valid frame — SOF-scan must re-sync and decode. */
static void test_loopback_resync_after_garbage(void) {
    // Push raw garbage bytes directly into Serial2 TX — they arrive on RX side.
    const uint8_t garbage[] = { 0x00u, 0x55u, 0xFFu, 0x12u, 0x34u };
    Serial2.write(garbage, sizeof(garbage));
    delay(5u);

    fillRandom(0xFEFEFEFEul);
    delay(30u);
    combus_tx_update(&txComBus);
    delay(kRxPollMs);
    combus_rx_update();

    const ComBusFrame* snap = combus_rx_snapshot(kLinkIdx);
    TEST_ASSERT_NOT_NULL_MESSAGE(snap, "No snapshot after re-sync — SOF scan may be broken");
    assertFrameMatchesBus(snap, &txComBus);
}

/** Monkey loopback — N random payloads, each must survive TX→RX intact. */
static void test_loopback_monkey(void) {
    for (uint8_t pass = 0u; pass < kMonkeyPasses; ++pass) {
        fillRandom((uint32_t)pass * 0x1234ABCDul);

        delay(30u);
        combus_tx_update(&txComBus);
        delay(kRxPollMs);
        combus_rx_update();

        const ComBusFrame* snap = combus_rx_snapshot(kLinkIdx);
        TEST_ASSERT_NOT_NULL_MESSAGE(snap, "Monkey loopback: no snapshot");
        assertFrameMatchesBus(snap, &txComBus);
    }
}


// =============================================================================
// GROUP C — VIEW IDENTITY (Phase 3 / A13)
//
// Validates the central invariant of Solution A: the 3 view-specific
// enums (AnalogComBusID, AnalogComBusSystemID, AnalogComBusBaseID and
// their digital counterparts) must reference the SAME runtime object
// in AnalogComBusArray[] / DigitalComBusArray[]. Comparing only the
// numeric values would NOT prove this — two independent arrays
// initialised identically would pass a value-only check. We compare
// POINTERS (TEST_ASSERT_EQUAL_PTR) to prove memory identity.
//
// This is the C++-runtime counterpart of the Python test
// `test_render_ids_header_emits_distinct_per_bus_values`. The Python
// test verifies the generator's output; this C++ test verifies the
// actual compiled code (and would catch any future build-system bug
// that re-mapped the enums independently).
//
// Channel choices (volvo_A60H_bruder / dumper_truck):
//   - BRAKE_BUS (BASE analog, id=0): present in all 3 views.
//   - DUMP_STICK (SYSTEM analog, id=18): present in combus + combus_system,
//     absent from combus_base (BASE-only view).
//   - CRUISE_ACTIVE (BASE digital, id=9): present in all 3 views.
//   - CRUISE_TOGGLE_BTN (SYSTEM digital, id=21): present in combus +
//     combus_system, absent from combus_base.
//
// The generated combus_generated/combus.cpp is added to the test env's
// source list via scripts/combus_test_add_generated_source.py (registered
// as `extra_scripts = pre:...` in the test_combus_loopback env section).
// This makes AnalogComBusArray / DigitalComBusArray available at link
// time, enabling the pointer-equality tests below.
// =============================================================================

/** Analog: a BASE channel must be the same object in all 3 views. */
static void test_same_memory_object_analog_base(void) {
    AnalogComBus& via_full   = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::BRAKE_BUS)];
    AnalogComBus& via_base = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusBaseID::BRAKE_BUS)];
    AnalogComBus& via_system  = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusSystemID::BRAKE_BUS)];

    // Pointer identity: the 3 views must reference the same object.
    TEST_ASSERT_EQUAL_PTR(&via_full, &via_base);
    TEST_ASSERT_EQUAL_PTR(&via_full, &via_system);

    // Write via one view, read via the others.
    via_full.value = 12345u;
    TEST_ASSERT_EQUAL_UINT16(12345u, via_base.value);
    TEST_ASSERT_EQUAL_UINT16(12345u, via_system.value);

    via_base.value = 6789u;
    TEST_ASSERT_EQUAL_UINT16(6789u, via_full.value);
    TEST_ASSERT_EQUAL_UINT16(6789u, via_system.value);
}

/** Analog: a SYSTEM-only channel must be the same object in combus + combus_system. */
static void test_same_memory_object_analog_system_only(void) {
    AnalogComBus& via_full  = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::DUMP_STICK)];
    AnalogComBus& via_system = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusSystemID::DUMP_STICK)];

    // Pointer identity: combus and combus_system must share the object.
    TEST_ASSERT_EQUAL_PTR(&via_full, &via_system);

    // Write via one view, read via the other.
    via_system.value = 999u;
    TEST_ASSERT_EQUAL_UINT16(999u, via_full.value);
}

/** Digital: a BASE channel must be the same object in all 3 views. */
static void test_same_memory_object_digital_base(void) {
    DigitalComBus& via_full   = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CRUISE_ACTIVE)];
    DigitalComBus& via_base = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusBaseID::CRUISE_ACTIVE)];
    DigitalComBus& via_system  = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusSystemID::CRUISE_ACTIVE)];

    TEST_ASSERT_EQUAL_PTR(&via_full, &via_base);
    TEST_ASSERT_EQUAL_PTR(&via_full, &via_system);

    via_full.value = true;
    TEST_ASSERT_TRUE(via_base.value);
    TEST_ASSERT_TRUE(via_system.value);

    via_base.value = false;
    TEST_ASSERT_FALSE(via_full.value);
    TEST_ASSERT_FALSE(via_system.value);
}

/** Digital: a SYSTEM-only channel must be the same object in combus + combus_system. */
static void test_same_memory_object_digital_system_only(void) {
    DigitalComBus& via_full  = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CRUISE_TOGGLE_BTN)];
    DigitalComBus& via_system = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusSystemID::CRUISE_TOGGLE_BTN)];

    TEST_ASSERT_EQUAL_PTR(&via_full, &via_system);

    via_system.value = true;
    TEST_ASSERT_TRUE(via_full.value);
}

/** Prefix alignment: the 3 enums must agree on numeric values for shared channels. */
static void test_id_prefix_alignment_analog(void) {
    // BASE channels: present in all 3 views, must have the same numeric id.
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::BRAKE_BUS),
                            static_cast<uint8_t>(AnalogComBusBaseID::BRAKE_BUS));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::BRAKE_BUS),
                            static_cast<uint8_t>(AnalogComBusSystemID::BRAKE_BUS));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::THROTTLE_BUS),
                            static_cast<uint8_t>(AnalogComBusBaseID::THROTTLE_BUS));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::THROTTLE_BUS),
                            static_cast<uint8_t>(AnalogComBusSystemID::THROTTLE_BUS));

    // SYSTEM-only channels: present in combus + combus_system, must agree.
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::DUMP_STICK),
                            static_cast<uint8_t>(AnalogComBusSystemID::DUMP_STICK));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::STEERING_STICK),
                            static_cast<uint8_t>(AnalogComBusSystemID::STEERING_STICK));
}

/** Prefix alignment: digital counterpart. */
static void test_id_prefix_alignment_digital(void) {
    // BASE channels.
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::CRUISE_ACTIVE),
                            static_cast<uint8_t>(DigitalComBusBaseID::CRUISE_ACTIVE));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::CRUISE_ACTIVE),
                            static_cast<uint8_t>(DigitalComBusSystemID::CRUISE_ACTIVE));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::LIGHTS),
                            static_cast<uint8_t>(DigitalComBusBaseID::LIGHTS));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::LIGHTS),
                            static_cast<uint8_t>(DigitalComBusSystemID::LIGHTS));

    // SYSTEM-only channels.
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::CRUISE_TOGGLE_BTN),
                            static_cast<uint8_t>(DigitalComBusSystemID::CRUISE_TOGGLE_BTN));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::HORN_BTN),
                            static_cast<uint8_t>(DigitalComBusSystemID::HORN_BTN));
}


// =============================================================================
// GROUP D — WIRE-END SIZING (LY2)
//
// Verifies that combus_frame_encode() with a smaller analogWireEnd /
// digitalWireEnd produces a SHORTER buffer than with the full enum set:
// the wire payload is sized to the layer's prefix length instead of the
// full enum set.
//
// Expected sizes are computed from the generated CH_COUNT constants of
// each view (frameLen() helper), never hard-coded: they follow the
// profile when channels are added or removed.
// =============================================================================

static constexpr uint8_t kBaseAnalog    = static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT);
static constexpr uint8_t kBaseDigital   = static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT);
static constexpr uint8_t kSystemAnalog  = static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT);
static constexpr uint8_t kSystemDigital = static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT);

/** Encode with BASE wire-end — buffer must be shorter than FULL. */
static void test_wire_end_base_shorter_than_full(void) {
    fillRandom(0xD1D2D3D4ul);

    uint8_t lenBase = combus_frame_encode(kCfg, encodeBuf, &txComBus, 1u, kBaseAnalog, kBaseDigital);
    TEST_ASSERT_EQUAL_UINT8(frameLen(kBaseAnalog, kBaseDigital), lenBase);

    uint8_t lenFull = combus_frame_encode(kCfg, encodeBuf, &txComBus, 2u, kNAnalog, kNDigital);
    TEST_ASSERT_EQUAL_UINT8(frameLen(kNAnalog, kNDigital), lenFull);

    // The BASE buffer must be SHORTER than the FULL buffer.
    TEST_ASSERT_LESS_THAN_UINT8(lenFull, lenBase);
}

/** Encode with SYSTEM wire-end — buffer sized by the SYSTEM view, never longer than FULL. */
static void test_wire_end_system_sized_by_view(void) {
    fillRandom(0xE1E2E3E4ul);

    uint8_t lenSystem = combus_frame_encode(kCfg, encodeBuf, &txComBus, 3u, kSystemAnalog, kSystemDigital);
    TEST_ASSERT_EQUAL_UINT8(frameLen(kSystemAnalog, kSystemDigital), lenSystem);

    uint8_t lenFull = combus_frame_encode(kCfg, encodeBuf, &txComBus, 4u, kNAnalog, kNDigital);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(lenFull, lenSystem);
}

/** Encode with wire-end = 0 — buffer must be the minimum (header + CRC). */
static void test_wire_end_zero_is_minimum(void) {
    fillRandom(0xF1F2F3F4ul);

    uint8_t lenZero = combus_frame_encode(kCfg, encodeBuf, &txComBus, 5u, 0u, 0u);

    // Minimum frame: SOF + header, no digital byte, no analog value, CRC.
    TEST_ASSERT_EQUAL_UINT8(frameLen(0u, 0u), lenZero);
}

/** Encode with wire-end > cfg — must clamp to the cfg counts (no overflow). */
static void test_wire_end_clamped_to_cfg(void) {
    fillRandom(0xA1A2A3A4ul);

    uint8_t lenClamped = combus_frame_encode(kSmallCfg, encodeBuf, &txComBus, 6u, 200u, 200u);

    // Clamped to kSmallCfg (4 analog, 6 digital).
    TEST_ASSERT_EQUAL_UINT8(frameLen(kSmallCfg.nAnalog, kSmallCfg.nDigital), lenClamped);
}


// =============================================================================
// GROUP G — RUNLEVEL ROUND-TRIP (RL3)
//
// Verifies that the RUNLEVEL analog channel survives an encode→decode
// round-trip without any special-cased handling in the codec. The
// codec stays 100% channel-agnostic — RUNLEVEL is just another
// analog channel like THROTTLE_STICK or DUMP_STICK.
// =============================================================================

/** RUNLEVEL round-trip — encode→decode preserves the RUNLEVEL analog value. */
static void test_runlevel_roundtrip_via_channel(void) {
    // Write RUNLEVEL via the generic analog accessor (mainboard pattern).
    bool written = combus_set_analog(txComBus, AnalogComBusID::RUNLEVEL,
                                     (uint16_t)RunLevel::STARTING, ChanLayer::SYSTEM);
    TEST_ASSERT_TRUE_MESSAGE(written, "RUNLEVEL write denied (layer check)");

    uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 7u, kNAnalog, kNDigital);
    TEST_ASSERT_GREATER_THAN_UINT8(0u, len);

    bool ok = combus_frame_decode(kCfg, &rxFrame, encodeBuf, len);
    TEST_ASSERT_TRUE(ok);

    // The decoded analog[RUNLEVEL] must equal the encoded value.
    TEST_ASSERT_EQUAL_UINT16((uint16_t)RunLevel::STARTING,
                             rxFrame.analog[static_cast<uint8_t>(AnalogComBusID::RUNLEVEL)]);
}


// =============================================================================
// GROUP E — MD5 POINTER RESOLUTION BY LAYER (LY4)
//
// Verifies that combus_protocol_init() resolves the per-link
// expectedMd5 pointer from link->layer (BASE / SYSTEM / BOARD), and
// that combus_handshake_compareAndLog() matches correctly when given
// the right local MD5 pointer.  Same pattern as Group D — verify the
// resolution at init, not the per-frame behaviour.
//
// The 3 MD5 arrays are generated by scripts/combus_builder/md5.py
// under <build_dir>/combus_generated/ and exposed via combus::wire:
//   - kCombusComBusMd5       (FULL view, machine node default)
//   - kCombusBaseComBusMd5 (BASE view, RF link)
//   - kCombusSystemComBusMd5  (SYSTEM view, extension board link)
//
// The 3 arrays MUST be distinct (different views hash different
// channel sets).  This is the structural invariant that LY4 relies on — if
// two views ever produced the same MD5, the per-layer resolution
// would be a no-op and the test would catch it.
// =============================================================================

/** The 3 MD5 arrays must be distinct (different views hash different inputs). */
static void test_md5_arrays_are_distinct(void) {
    // Pointer identity: the 3 arrays live at different addresses.
    TEST_ASSERT_NOT_EQUAL(
        (const void*)combus::wire::kCombusComBusMd5,
        (const void*)combus::wire::kCombusBaseComBusMd5);
    TEST_ASSERT_NOT_EQUAL(
        (const void*)combus::wire::kCombusComBusMd5,
        (const void*)combus::wire::kCombusSystemComBusMd5);
    TEST_ASSERT_NOT_EQUAL(
        (const void*)combus::wire::kCombusBaseComBusMd5,
        (const void*)combus::wire::kCombusSystemComBusMd5);

    // Content identity: at least one byte must differ between any pair.
    bool baseDiffersFromFull = false;
    bool systemDiffersFromFull  = false;
    bool systemDiffersFromBase = false;
    for (uint8_t i = 0u; i < 16u; ++i) {
        if (combus::wire::kCombusComBusMd5[i]       != combus::wire::kCombusBaseComBusMd5[i]) { baseDiffersFromFull = true; }
        if (combus::wire::kCombusComBusMd5[i]       != combus::wire::kCombusSystemComBusMd5[i])  { systemDiffersFromFull  = true; }
        if (combus::wire::kCombusBaseComBusMd5[i] != combus::wire::kCombusSystemComBusMd5[i])  { systemDiffersFromBase = true; }
    }
    TEST_ASSERT_TRUE_MESSAGE(baseDiffersFromFull,  "BASE MD5 must differ from FULL MD5");
    TEST_ASSERT_TRUE_MESSAGE(systemDiffersFromFull,   "SYSTEM MD5 must differ from FULL MD5");
    TEST_ASSERT_TRUE_MESSAGE(systemDiffersFromBase, "SYSTEM MD5 must differ from BASE MD5");
}

/** combus_protocol_init() with BASE layer must resolve to kCombusBaseComBusMd5. */
static void test_md5_resolution_base(void) {
    static CombusHandshakeContext ctx = {};
    static ComBusLink link = {};

    link.name        = "test_remote";
    link.com         = nullptr;  // not used by the MD5 resolution path
    link.txCfg       = std::nullopt;  // skip TX init
    link.rxCfg       = std::nullopt;  // skip RX init
    link.handshakeCtx = &ctx;
    link.layer       = ChanLayer::BASE;

    // The MD5 resolution happens BEFORE the TX/RX init, so a null com
    // is safe — the switch on link->layer runs first.
    combus_protocol_init(kLinkIdx, &link);

    TEST_ASSERT_EQUAL_PTR(
        (const void*)combus::wire::kCombusBaseComBusMd5,
        (const void*)ctx.expectedMd5);
}

/** combus_protocol_init() with SYSTEM layer must resolve to kCombusSystemComBusMd5. */
static void test_md5_resolution_system(void) {
    static CombusHandshakeContext ctx = {};
    static ComBusLink link = {};

    link.name        = "test_local";
    link.com         = nullptr;
    link.txCfg       = std::nullopt;
    link.rxCfg       = std::nullopt;
    link.handshakeCtx = &ctx;
    link.layer       = ChanLayer::SYSTEM;

    combus_protocol_init(kLinkIdx, &link);

    TEST_ASSERT_EQUAL_PTR(
        (const void*)combus::wire::kCombusSystemComBusMd5,
        (const void*)ctx.expectedMd5);
}

/** combus_protocol_init() with BOARD layer must resolve to kCombusComBusMd5 (FULL). */
static void test_md5_resolution_board(void) {
    static CombusHandshakeContext ctx = {};
    static ComBusLink link = {};

    link.name        = "test_system";
    link.com         = nullptr;
    link.txCfg       = std::nullopt;
    link.rxCfg       = std::nullopt;
    link.handshakeCtx = &ctx;
    link.layer       = ChanLayer::BOARD;

    combus_protocol_init(kLinkIdx, &link);

    TEST_ASSERT_EQUAL_PTR(
        (const void*)combus::wire::kCombusComBusMd5,
        (const void*)ctx.expectedMd5);
}

// Chantier 12.6 (cleanup): Group F (FS1 failsafe re-arm) REMOVED — link
// health is now monitored by the per-backend *_LINK_LOST contributors
// (PS4_DS4_BT_LINK_LOST, UART_LINK_LOST, ...) aggregated into
// REMOTE_LINK_LOST, which feeds the top-level FAILSAFE aggregator
// (see failsafe_module.md §12.5 new design).
//
// The replacement tests (link health via REMOTE_LINK_LOST aggregation)
// will be added in a future chantier once the contract is finalised.

/** combus_handshake_compareAndLog() must match when given the right local MD5. */
static void test_md5_compare_match_with_correct_local(void) {
    // Wire MD5 = BASE view (simulating a peer that speaks BASE).
    const uint8_t* wireMd5 = combus::wire::kCombusBaseComBusMd5;

    // Compare against the BASE local MD5 — must match.
    bool match = combus_handshake_compareAndLog(
        combus::wire::kCombusBaseComBusMd5,
        wireMd5,
        combus::wire::kProjectVersionMajor,
        combus::wire::kProjectVersionMinor);
    TEST_ASSERT_TRUE_MESSAGE(match, "BASE local MD5 must match BASE wire MD5");

    // Compare against the FULL local MD5 — must NOT match (different views).
    match = combus_handshake_compareAndLog(
        combus::wire::kCombusComBusMd5,
        wireMd5,
        combus::wire::kProjectVersionMajor,
        combus::wire::kProjectVersionMinor);
    TEST_ASSERT_FALSE_MESSAGE(match, "FULL local MD5 must NOT match BASE wire MD5");

    // Compare against the SYSTEM local MD5 — must NOT match (different views).
    match = combus_handshake_compareAndLog(
        combus::wire::kCombusSystemComBusMd5,
        wireMd5,
        combus::wire::kProjectVersionMajor,
        combus::wire::kProjectVersionMinor);
    TEST_ASSERT_FALSE_MESSAGE(match, "SYSTEM local MD5 must NOT match BASE wire MD5");
}


// =============================================================================
// RUNNER
// =============================================================================

void setup() {
    delay(2000u);  // let the board stabilize before Unity starts
    UNITY_BEGIN();

    // --- Group A: codec round-trip (no hardware needed) ---
    RUN_TEST(test_codec_single_roundtrip);
    RUN_TEST(test_codec_seq_zero_refused);
    RUN_TEST(test_codec_flags_reserved);
    RUN_TEST(test_codec_crc_reject);
    RUN_TEST(test_codec_monkey_roundtrip);

    // --- Group B: UART loopback (TX↔RX jumper required) ---
    RUN_TEST(test_loopback_init);
    RUN_TEST(test_loopback_single_frame);
    RUN_TEST(test_loopback_resync_after_garbage);
    RUN_TEST(test_loopback_monkey);

    // --- Group C: view identity (Phase 3 / A13) ---
    // Memory identity (pointer equality) + prefix alignment (numeric).
    // The generated combus_generated/combus.cpp is added to the test
    // env's source list via scripts/combus_test_add_generated_source.py.
    RUN_TEST(test_same_memory_object_analog_base);
    RUN_TEST(test_same_memory_object_analog_system_only);
    RUN_TEST(test_same_memory_object_digital_base);
    RUN_TEST(test_same_memory_object_digital_system_only);
    RUN_TEST(test_id_prefix_alignment_analog);
    RUN_TEST(test_id_prefix_alignment_digital);

    // --- Group D: wire-end sizing (LY2) ---
    RUN_TEST(test_wire_end_base_shorter_than_full);
    RUN_TEST(test_wire_end_system_sized_by_view);
    RUN_TEST(test_wire_end_zero_is_minimum);
    RUN_TEST(test_wire_end_clamped_to_cfg);

    // --- Group G: RUNLEVEL round-trip (RL3) ---
    // Verifies that the RUNLEVEL analog channel survives an encode→decode
    // round-trip without any special-cased handling in the codec.
    RUN_TEST(test_runlevel_roundtrip_via_channel);

    // --- Group E: MD5 pointer resolution by layer (LY4) ---
    RUN_TEST(test_md5_arrays_are_distinct);
    RUN_TEST(test_md5_resolution_base);
    RUN_TEST(test_md5_resolution_system);
    RUN_TEST(test_md5_resolution_board);
    RUN_TEST(test_md5_compare_match_with_correct_local);

    // Chantier 12.6: Group F (FS1 failsafe re-arm) removed — see
    // header docstring above (5 independent test groups A/B/C/D/E).

    UNITY_END();
}

void loop() {}

// EOF test_combus_loopback.cpp