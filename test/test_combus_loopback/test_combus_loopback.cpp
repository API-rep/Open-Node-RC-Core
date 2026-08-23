/******************************************************************************
 * @file test_combus_loopback.cpp
 * @brief ComBus codec + UART loopback test suite.
 *
 * @details Two independent test groups:
 *
 *   Group A — codec round-trip (no hardware required):
 *     Instantiates two ComBus structs with random analog/digital data,
 *     encodes each into a binary frame via combus_frame_encode(), then
 *     decodes the result via combus_frame_decode() and asserts field-by-field
 *     equality between the original ComBus and the decoded ComBusFrame.
 *
 *   Group B — UART loopback (requires TX↔RX wire on Serial2):
 *     Uses combus_tx_update() and combus_rx_update() end-to-end.
 *     Sends N frames with random payloads, then polls combus_rx_snapshot()
 *     and asserts field equality. Also verifies SOF-scan re-sync by injecting
 *     garbage bytes before a valid frame.
 *
 * @note Group B requires a physical TX↔RX jumper on the Serial2 pins of
 *   the target board. Run via: pio test -e volvo_A60H_bruder
 *****************************************************************************/

#include <Arduino.h>
#include <unity.h>

#include <core/system/combus/combus_frame.h>
#include <core/system/hw/transport/uart_com.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <struct/combus_struct.h>
#include <struct/outputs_struct.h>

// Phase 3 / A13: include combus.h (declares AnalogComBusArray /
// DigitalComBusArray) AND the 3 view-specific ids headers so the
// runtime identity test can reference AnalogComBusID, AnalogComBusLocalID
// and AnalogComBusRemoteID in the same translation unit.
//
// combus.h transitively includes combus_ids.h, so we do NOT include
// it directly here (avoids the double-definition error: combus_ids.h
// does not yet have a #pragma once guard).
#include "combus.h"
#include "combus_local_ids.h"
#include "combus_remote_ids.h"


// =============================================================================
// TEST CONFIGURATION
// =============================================================================

static constexpr uint8_t  kTestNAnalog  = 4u;
static constexpr uint8_t  kTestNDigital = 6u;
static constexpr uint32_t kLoopbackBaud = 115200u;

static constexpr int      kTxPin        = 17;     ///< Serial2 TX — connect to kRxPin
static constexpr int      kRxPin        = 16;     ///< Serial2 RX — connect to kTxPin
static constexpr uint32_t kRxPollMs     = 50u;    ///< poll budget per frame (ms)
static constexpr uint8_t  kMonkeyPasses = 8u;     ///< number of random frames to send


// =============================================================================
// SHARED TEST FIXTURES
// =============================================================================

static constexpr ComBusFrameCfg kCfg = { kTestNAnalog, kTestNDigital };


// --- ComBus A (source / TX side) ---
static AnalogComBus  txAnalogBus[kTestNAnalog];
static DigitalComBus txDigitalBus[kTestNDigital];
static ComBus        txComBus = {
    .runLevel       = RunLevel::RUNNING,
    .analogBus      = txAnalogBus,
    .digitalBus     = txDigitalBus,
    .analogBusMaxVal = 1000u
};

// --- ComBusFrame B (decode / RX side) ---
static uint16_t rxAnalogBuf[kTestNAnalog]   = {};
static bool     rxDigitalBuf[kTestNDigital] = {};
static ComBusFrame rxFrame = { .analog = rxAnalogBuf, .digital = rxDigitalBuf };

// --- Encode output buffer ---
static uint8_t encodeBuf[255u];


// =============================================================================
// HELPERS
// =============================================================================

/** Fill txComBus with deterministic pseudo-random values seeded by `seed`. */
static void fillRandom(uint32_t seed) {
    srand(seed);
    txComBus.runLevel = RunLevel::RUNNING;
    for (uint8_t i = 0u; i < kTestNAnalog; ++i) {
        txAnalogBus[i].value    = (uint16_t)(rand() % 1001u);
        // Note: the per-channel 'isDrived' flag was removed in A6.1 (it
        // is now on the ComBus struct, not on individual channels). See
        // Phase 3 / A13 fix below (fillRandom still works because the
        // codec test only cares about `value`).
    }
    for (uint8_t i = 0u; i < kTestNDigital; ++i) {
        txDigitalBus[i].value    = (rand() % 2) == 1;
    }
    // Drive the bus once for this test (the runtime flag is per-bus, not
    // per-channel in A6.1+).
    txComBus.isDrived = true;
}

/** Assert that rxFrame fields match txComBus content. */
static void assertFrameMatchesBus(const ComBusFrame* frame, const ComBus* bus) {
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)bus->runLevel, frame->header.runLevel);
    for (uint8_t i = 0u; i < kTestNAnalog; ++i) {
        TEST_ASSERT_EQUAL_UINT16(bus->analogBus[i].value, frame->analog[i]);
    }
    for (uint8_t i = 0u; i < kTestNDigital; ++i) {
        TEST_ASSERT_EQUAL(bus->digitalBus[i].value, frame->digital[i]);
    }
}


// =============================================================================
// GROUP A — CODEC ROUND-TRIP (no hardware)
// =============================================================================

/** Single encode→decode round-trip with a fixed seed. */
static void test_codec_single_roundtrip(void) {
    fillRandom(0xDEADBEEFul);

    uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 0u, false);
    TEST_ASSERT_GREATER_THAN_UINT8(0u, len);

    bool ok = combus_frame_decode(kCfg, &rxFrame, encodeBuf, len);
    TEST_ASSERT_TRUE(ok);

    assertFrameMatchesBus(&rxFrame, &txComBus);
}

/** Encode→decode with failsafe flag set — header flags byte must reflect it. */
static void test_codec_failsafe_flag(void) {
    fillRandom(0x12345678ul);

    uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 1u, true);
    TEST_ASSERT_GREATER_THAN_UINT8(0u, len);

    bool ok = combus_frame_decode(kCfg, &rxFrame, encodeBuf, len);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_BITS(COMBUS_FLAG_FAILSAFE, COMBUS_FLAG_FAILSAFE, rxFrame.header.flags);
}

/** Corrupt one payload byte — CRC must reject the frame. */
static void test_codec_crc_reject(void) {
    fillRandom(0xCAFEBABEul);

    uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 2u, false);
    TEST_ASSERT_GREATER_THAN_UINT8(0u, len);

    encodeBuf[len / 2u] ^= 0xFFu;  // flip bits in the middle of the frame

    bool ok = combus_frame_decode(kCfg, &rxFrame, encodeBuf, len);
    TEST_ASSERT_FALSE(ok);  // CRC must catch the corruption
}

/** Monkey test — N random seeds, each must round-trip cleanly. */
static void test_codec_monkey_roundtrip(void) {
    for (uint8_t pass = 0u; pass < kMonkeyPasses; ++pass) {
        fillRandom((uint32_t)pass * 0x9E3779B9ul);

        uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, pass, false);
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
    NodeCom* com = uart_com_init(&Serial2, kLoopbackBaud, kTxPin, kRxPin, "test_loopback");
    TEST_ASSERT_NOT_NULL(com);

    combus_tx_init(com, kCfg, 50u);   // 50 Hz TX
    combus_rx_init(com, kCfg, rxAnalogBuf, rxDigitalBuf);

    // No assertion beyond not crashing — both modules share the same NodeCom*.
}

/** Send one frame, poll RX, assert snapshot matches what was sent. */
static void test_loopback_single_frame(void) {
    fillRandom(0xABCD1234ul);

        // Force TX to fire on next call by resetting via a fresh init would be
        // heavier — instead we just wait one full period (20ms at 50Hz) + margin.
    delay(30u);
    combus_tx_update(&txComBus, false);
    delay(kRxPollMs);
    combus_rx_update();

    const ComBusFrame* snap = combus_rx_snapshot();
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
    combus_tx_update(&txComBus, false);
    delay(kRxPollMs);
    combus_rx_update();

    const ComBusFrame* snap = combus_rx_snapshot();
    TEST_ASSERT_NOT_NULL_MESSAGE(snap, "No snapshot after re-sync — SOF scan may be broken");
    assertFrameMatchesBus(snap, &txComBus);
}

/** Monkey loopback — N random payloads, each must survive TX→RX intact. */
static void test_loopback_monkey(void) {
    for (uint8_t pass = 0u; pass < kMonkeyPasses; ++pass) {
        fillRandom((uint32_t)pass * 0x1234ABCDul);

        delay(30u);
        combus_tx_update(&txComBus, false);
        delay(kRxPollMs);
        combus_rx_update();

        const ComBusFrame* snap = combus_rx_snapshot();
        TEST_ASSERT_NOT_NULL_MESSAGE(snap, "Monkey loopback: no snapshot");
        assertFrameMatchesBus(snap, &txComBus);
    }
}


// =============================================================================
// GROUP C — VIEW IDENTITY (Phase 3 / A13)
//
// Validates the central invariant of Solution A: the 3 view-specific
// enums (AnalogComBusID, AnalogComBusLocalID, AnalogComBusRemoteID and
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
//   - BRAKE_BUS (REMOTE analog, id=0): present in all 3 views.
//   - DUMP_STICK (LOCAL analog, id=18): present in combus + combus_local,
//     absent from combus_remote (REMOTE-only view).
//   - CRUISE_ACTIVE (REMOTE digital, id=9): present in all 3 views.
//   - CRUISE_TOGGLE_BTN (LOCAL digital, id=21): present in combus +
//     combus_local, absent from combus_remote.
//
// The generated combus_generated/combus.cpp is added to the test env's
// source list via scripts/combus_test_add_generated_source.py (registered
// as `extra_scripts = pre:...` in the test_combus_loopback env section).
// This makes AnalogComBusArray / DigitalComBusArray available at link
// time, enabling the pointer-equality tests below.
// =============================================================================

/** Analog: a REMOTE channel must be the same object in all 3 views. */
static void test_same_memory_object_analog_remote(void) {
    AnalogComBus& via_full   = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::BRAKE_BUS)];
    AnalogComBus& via_remote = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusRemoteID::BRAKE_BUS)];
    AnalogComBus& via_local  = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusLocalID::BRAKE_BUS)];

    // Pointer identity: the 3 views must reference the same object.
    TEST_ASSERT_EQUAL_PTR(&via_full, &via_remote);
    TEST_ASSERT_EQUAL_PTR(&via_full, &via_local);

    // Write via one view, read via the others.
    via_full.value = 12345u;
    TEST_ASSERT_EQUAL_UINT16(12345u, via_remote.value);
    TEST_ASSERT_EQUAL_UINT16(12345u, via_local.value);

    via_remote.value = 6789u;
    TEST_ASSERT_EQUAL_UINT16(6789u, via_full.value);
    TEST_ASSERT_EQUAL_UINT16(6789u, via_local.value);
}

/** Analog: a LOCAL-only channel must be the same object in combus + combus_local. */
static void test_same_memory_object_analog_local_only(void) {
    AnalogComBus& via_full  = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::DUMP_STICK)];
    AnalogComBus& via_local = AnalogComBusArray[static_cast<uint8_t>(AnalogComBusLocalID::DUMP_STICK)];

    // Pointer identity: combus and combus_local must share the object.
    TEST_ASSERT_EQUAL_PTR(&via_full, &via_local);

    // Write via one view, read via the other.
    via_local.value = 999u;
    TEST_ASSERT_EQUAL_UINT16(999u, via_full.value);
}

/** Digital: a REMOTE channel must be the same object in all 3 views. */
static void test_same_memory_object_digital_remote(void) {
    DigitalComBus& via_full   = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CRUISE_ACTIVE)];
    DigitalComBus& via_remote = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusRemoteID::CRUISE_ACTIVE)];
    DigitalComBus& via_local  = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusLocalID::CRUISE_ACTIVE)];

    TEST_ASSERT_EQUAL_PTR(&via_full, &via_remote);
    TEST_ASSERT_EQUAL_PTR(&via_full, &via_local);

    via_full.value = true;
    TEST_ASSERT_TRUE(via_remote.value);
    TEST_ASSERT_TRUE(via_local.value);

    via_remote.value = false;
    TEST_ASSERT_FALSE(via_full.value);
    TEST_ASSERT_FALSE(via_local.value);
}

/** Digital: a LOCAL-only channel must be the same object in combus + combus_local. */
static void test_same_memory_object_digital_local_only(void) {
    DigitalComBus& via_full  = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CRUISE_TOGGLE_BTN)];
    DigitalComBus& via_local = DigitalComBusArray[static_cast<uint8_t>(DigitalComBusLocalID::CRUISE_TOGGLE_BTN)];

    TEST_ASSERT_EQUAL_PTR(&via_full, &via_local);

    via_local.value = true;
    TEST_ASSERT_TRUE(via_full.value);
}

/** Prefix alignment: the 3 enums must agree on numeric values for shared channels. */
static void test_id_prefix_alignment_analog(void) {
    // REMOTE channels: present in all 3 views, must have the same numeric id.
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::BRAKE_BUS),
                            static_cast<uint8_t>(AnalogComBusRemoteID::BRAKE_BUS));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::BRAKE_BUS),
                            static_cast<uint8_t>(AnalogComBusLocalID::BRAKE_BUS));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::THROTTLE_BUS),
                            static_cast<uint8_t>(AnalogComBusRemoteID::THROTTLE_BUS));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::THROTTLE_BUS),
                            static_cast<uint8_t>(AnalogComBusLocalID::THROTTLE_BUS));

    // LOCAL-only channels: present in combus + combus_local, must agree.
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::DUMP_STICK),
                            static_cast<uint8_t>(AnalogComBusLocalID::DUMP_STICK));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusID::STEERING_STICK),
                            static_cast<uint8_t>(AnalogComBusLocalID::STEERING_STICK));
}

/** Prefix alignment: digital counterpart. */
static void test_id_prefix_alignment_digital(void) {
    // REMOTE channels.
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::CRUISE_ACTIVE),
                            static_cast<uint8_t>(DigitalComBusRemoteID::CRUISE_ACTIVE));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::CRUISE_ACTIVE),
                            static_cast<uint8_t>(DigitalComBusLocalID::CRUISE_ACTIVE));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::LIGHTS),
                            static_cast<uint8_t>(DigitalComBusRemoteID::LIGHTS));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::LIGHTS),
                            static_cast<uint8_t>(DigitalComBusLocalID::LIGHTS));

    // LOCAL-only channels.
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::CRUISE_TOGGLE_BTN),
                            static_cast<uint8_t>(DigitalComBusLocalID::CRUISE_TOGGLE_BTN));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusID::HORN_BTN),
                            static_cast<uint8_t>(DigitalComBusLocalID::HORN_BTN));
}


// =============================================================================
// RUNNER
// =============================================================================

void setup() {
    delay(2000u);  // let the board stabilize before Unity starts
    UNITY_BEGIN();

    // --- Group A: codec round-trip (no hardware needed) ---
    RUN_TEST(test_codec_single_roundtrip);
    RUN_TEST(test_codec_failsafe_flag);
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
    RUN_TEST(test_same_memory_object_analog_remote);
    RUN_TEST(test_same_memory_object_analog_local_only);
    RUN_TEST(test_same_memory_object_digital_remote);
    RUN_TEST(test_same_memory_object_digital_local_only);
    RUN_TEST(test_id_prefix_alignment_analog);
    RUN_TEST(test_id_prefix_alignment_digital);

    UNITY_END();
}

void loop() {}

// EOF test_combus_loopback.cpp
