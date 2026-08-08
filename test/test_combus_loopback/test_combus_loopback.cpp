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

#include <core/system/combus/frame/combus_frame.h>
#include <core/system/hw/transport/uart_com.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/combus/combus_defs.h>
#include <core/system/combus/frame/combus_frame_defs.h>
#include <core/system/combus/frame/combus_handshake.h>
#include <core/system/combus/frame/combus_handshake_rx.h>  // combus_handshake_compareAndLog



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

		// seq == 0 is RESERVED for handshake frames (see combus_frame.h),
		// combus_frame_encode() must reject it.
	uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 0u, false);
	TEST_ASSERT_EQUAL_UINT8(0u, len);

		// Regular round-trip with a valid seq (1).
	len = combus_frame_encode(kCfg, encodeBuf, &txComBus, 1u, false);
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

/** Monkey test — N random seeds, each must round-trip cleanly.
 *
 *  Note: seq starts at `pass + 1u` (not `pass`) because seq == 0 is RESERVED
 *  for handshake frames; combus_frame_encode() rejects it.
 */
static void test_codec_monkey_roundtrip(void) {
	for (uint8_t pass = 0u; pass < kMonkeyPasses; ++pass) {
		fillRandom((uint32_t)pass * 0x9E3779B9ul);

		uint8_t len = combus_frame_encode(kCfg, encodeBuf, &txComBus, (uint8_t)(pass + 1u), false);
		TEST_ASSERT_GREATER_THAN_UINT8(0u, len);

		bool ok = combus_frame_decode(kCfg, &rxFrame, encodeBuf, len);
		TEST_ASSERT_TRUE_MESSAGE(ok, "Monkey round-trip failed");

		assertFrameMatchesBus(&rxFrame, &txComBus);
	}
}


// =============================================================================
// GROUP C — HANDSHAKE CONTRACT-VALIDATED FLAG LIFECYCLE
// =============================================================================
//
// P2 (WIP combus_v2 §5.2) — covers the lifecycle of
// combus_handshake_is_contract_validated() and its wiring:
//   - boot / combus_rx_init()           -> flag cleared (false)
//   - match on combus_handshake_compareAndLog() -> flag set (true)
//   - mismatch                          -> flag untouched (stays at previous value)
//
// Tests use the codec-level accessors directly — no UART traffic needed.
// Each test starts with a fresh combus_rx_init() to guarantee a known
// starting state for the static flag.

static void test_handshake_contract_flag_cleared_on_init(void) {
    NodeCom* com = uart_com_init(&Serial2, kLoopbackBaud, kTxPin, kRxPin, "test_loopback");
    TEST_ASSERT_NOT_NULL(com);

    combus_rx_init(com, kCfg, rxAnalogBuf, rxDigitalBuf);
    TEST_ASSERT_FALSE_MESSAGE(
        combus_handshake_is_contract_validated(),
        "flag must be cleared by combus_rx_init()");
}

static void test_handshake_contract_flag_set_on_match(void) {
    NodeCom* com = uart_com_init(&Serial2, kLoopbackBaud, kTxPin, kRxPin, "test_loopback");
    TEST_ASSERT_NOT_NULL(com);

    combus_rx_init(com, kCfg, rxAnalogBuf, rxDigitalBuf);
    TEST_ASSERT_FALSE(combus_handshake_is_contract_validated());

    const bool matched = combus_handshake_compareAndLog(
        combus::wire::kCombusWireMd5,
        combus::wire::kProjectVersionMajor,
        combus::wire::kProjectVersionMinor);
    TEST_ASSERT_TRUE_MESSAGE(matched, "compare with local copy must match");
    TEST_ASSERT_TRUE_MESSAGE(
        combus_handshake_is_contract_validated(),
        "flag must be set after a successful match");
}

static void test_handshake_contract_flag_untouched_on_mismatch(void) {
    NodeCom* com = uart_com_init(&Serial2, kLoopbackBaud, kTxPin, kRxPin, "test_loopback");
    TEST_ASSERT_NOT_NULL(com);

    combus_rx_init(com, kCfg, rxAnalogBuf, rxDigitalBuf);
    TEST_ASSERT_FALSE(combus_handshake_is_contract_validated());

    // Flip one bit of the MD5 so the comparison fails.
    uint8_t badMd5[16];
    memcpy(badMd5, combus::wire::kCombusWireMd5, 16);
    badMd5[0] ^= 0x01u;

    const bool matched = combus_handshake_compareAndLog(
        badMd5,
        combus::wire::kProjectVersionMajor,
        combus::wire::kProjectVersionMinor);
    TEST_ASSERT_FALSE_MESSAGE(matched, "flipped MD5 must NOT match");
    TEST_ASSERT_FALSE_MESSAGE(
        combus_handshake_is_contract_validated(),
        "mismatch must not flip the flag");
}

/** Full lifecycle: init -> false, match -> true, init again -> false.
 *  Covers the reset-on-transport-reinit semantics end-to-end.
 */
static void test_handshake_contract_full_cycle(void) {
    NodeCom* com = uart_com_init(&Serial2, kLoopbackBaud, kTxPin, kRxPin, "test_loopback");
    TEST_ASSERT_NOT_NULL(com);

    // 1. fresh init -> flag cleared
    combus_rx_init(com, kCfg, rxAnalogBuf, rxDigitalBuf);
    TEST_ASSERT_FALSE_MESSAGE(
        combus_handshake_is_contract_validated(),
        "fresh init must clear the flag");

    // 2. match -> flag set
    const bool matched = combus_handshake_compareAndLog(
        combus::wire::kCombusWireMd5,
        combus::wire::kProjectVersionMajor,
        combus::wire::kProjectVersionMinor);
    TEST_ASSERT_TRUE_MESSAGE(matched, "compare with local copy must match");
    TEST_ASSERT_TRUE_MESSAGE(
        combus_handshake_is_contract_validated(),
        "flag must be set after a successful match");

    // 3. re-init -> flag cleared again (transport lifecycle reset)
    combus_rx_init(com, kCfg, rxAnalogBuf, rxDigitalBuf);
    TEST_ASSERT_FALSE_MESSAGE(
        combus_handshake_is_contract_validated(),
        "re-init must clear the flag back to false");
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

	// --- Group C: handshake contract-validated flag lifecycle (P2) ---
	// No extra hardware needed — uses the codec-level accessors directly.
	RUN_TEST(test_handshake_contract_flag_cleared_on_init);
	RUN_TEST(test_handshake_contract_flag_set_on_match);
	RUN_TEST(test_handshake_contract_flag_untouched_on_mismatch);
	RUN_TEST(test_handshake_contract_full_cycle);



	UNITY_END();
}

void loop() {}

// EOF test_combus_loopback.cpp
