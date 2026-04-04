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


// =============================================================================
// TEST CONFIGURATION
// =============================================================================

static constexpr uint8_t  kTestNAnalog  = 4u;
static constexpr uint8_t  kTestNDigital = 6u;
static constexpr uint8_t  kTestEnvId    = 42u;   ///< arbitrary envId for testing
static constexpr uint32_t kLoopbackBaud = 115200u;
static constexpr int      kTxPin        = 17;     ///< Serial2 TX — connect to kRxPin
static constexpr int      kRxPin        = 16;     ///< Serial2 RX — connect to kTxPin
static constexpr uint32_t kRxPollMs     = 50u;    ///< poll budget per frame (ms)
static constexpr uint8_t  kMonkeyPasses = 8u;     ///< number of random frames to send


// =============================================================================
// SHARED TEST FIXTURES
// =============================================================================

static constexpr ComBusFrameCfg kCfg = { kTestEnvId, kTestNAnalog, kTestNDigital };

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
        txAnalogBus[i].isDrived = true;
    }
    for (uint8_t i = 0u; i < kTestNDigital; ++i) {
        txDigitalBus[i].value    = (rand() % 2) == 1;
        txDigitalBus[i].isDrived = true;
    }
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

    UNITY_END();
}

void loop() {}

// EOF test_combus_loopback.cpp
