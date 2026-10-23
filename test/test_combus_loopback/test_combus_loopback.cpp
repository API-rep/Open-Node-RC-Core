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
 *   Group H — RX write identity by link layer (chantier 0.1):
 *     Re-initialises the loopback link with a target ComBus and a layer, then
 *     checks which channels the received frame is allowed to write: a BASE
 *     link writes BASE channels only, a SYSTEM link writes BASE and SYSTEM
 *     channels, a BOARD link gets the SYSTEM identity.  Same TX↔RX jumper as
 *     Group B (runs after test_loopback_init).
 *
 *   Group I — links from their description (chantier 0.4):
 *     combus_link_check() and combus_link_build() turn a ComDevMap (a ComDev
 *     wired on a ComPort) into a ComBusLink: layer from the scope, frame size
 *     from the matching view, TX / RX from the role of the end, and the
 *     refusals. No wiring needed: the port is never opened.
 *
 *   Group J — receiving link health (chantier 1.1):
 *     A receiving link is lost until its first frame and again after
 *     linkTimeoutMs of silence; com_rx_update() applies the received frames; a
 *     TX-only link never counts as lost.
 *     Same TX↔RX jumper as Group B.
 *
 *   Group K — denied writes are counted (chantier 1.1):
 *     A write refused by the layer check (a SYSTEM caller on a BOARD channel)
 *     increments combus_denied_writes(); an accepted write does not.
 *
 *   Group L — both_or merge, anti ping-pong (chantier 1.1b):
 *     A frame from a "peer" carrying FAILSAFE = true makes the merged FAILSAFE
 *     true while this board's own value is false; it falls back when the peer
 *     clears or goes silent; the frame this board sends back does not echo the
 *     peer's value; a board that never wrote the channel does not contribute.
 *     Same TX↔RX jumper as Group B.
 *
 *   Group M — link loss policy (chantier 1.1c):
 *     Rows 1 to 11 of the table validated by the owner: per link, its onLoss
 *     (default by scope: intra-node -> FAILSAFE, inter-node -> IDLE), its state
 *     (never seen, alive, lost), the 3 s startup grace for FAILSAFE (shortened
 *     here), and what com_rx_update() publishes on COM_LINK_LOST (IDLE) and
 *     FAILSAFE_COM_LINK (FAILSAFE).  Same TX↔RX jumper as Group B.
 *
 *   Group N — link contract, ACK and blocking (chantier 1.2):
 *     Rows 1 to 12 of the table validated by the owner: a link is UNKNOWN until
 *     a handshake with the same MD5 and major version (a bidirectional end needs
 *     the ACK of its request, a receive-only end the request itself); data
 *     frames are not applied while UNKNOWN; a bidirectional end sends no data
 *     while UNKNOWN; requests go on every 250 ms; 1 s without ACK or a silence drop the
 *     contract; a request is always answered, an ACK never; FAILSAFE no longer
 *     touches the contract (P4 removed).  Peer frames are forged on the wire.
 *     Same TX↔RX jumper as Group B.
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
 *     and that combus_handshake_compare() matches correctly when
 *     given the right local MD5 pointer.  Same pattern as Group D —
 *     verify the resolution at init, not the per-frame behaviour.
 *
 *   Group F — REMOVED (chantier 12.6 cleanup):
 *     The historical "failsafe trigger on combus link loss" test relied on
 *     the FS1 open-drain `isNotDrived` flag and a dedicated FS1 contributor
 *     channel.  Both have been removed in chantier 12.5/12.6: link health
 *     is now monitored at transport level by per-backend *_LINK_LOST
 *     contributors (PS4_DS4_BT_LINK_LOST, COM_LINK_LOST, ...) and
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
#include <core/system/combus/protocol/combus_links.h>    // combus_link_check(), combus_link_build() — Group I
#include <core/system/com/com_manager.h>                 // com_rx_update() — Group J
#include <core/system/combus/protocol/frame/combus_handshake.h> // CombusHandshakeContext — Groups E, N
#include <core/system/combus/protocol/frame/combus_handshake_rx.h> // combus_handshake_compare() — Group E
#include <core/system/combus/protocol/frame/combus_handshake_tx.h> // combus_handshake_tx_update() — Groups J, N

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
static NodeCom*          s_com    = nullptr;   ///< loopback transport, shared by Groups B and H


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
    s_com = com;

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
    combus_rx_update(&txComBus);

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
    combus_rx_update(&txComBus);

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
        combus_rx_update(&txComBus);

        const ComBusFrame* snap = combus_rx_snapshot(kLinkIdx);
        TEST_ASSERT_NOT_NULL_MESSAGE(snap, "Monkey loopback: no snapshot");
        assertFrameMatchesBus(snap, &txComBus);
    }
}


// =============================================================================
// GROUP H — RX WRITE IDENTITY BY LINK LAYER (chantier 0.1)
//
// A frame received on a link is applied onto the target ComBus with the layer
// of that link as write identity (combus_rx_init() `layer` parameter):
//   - BASE link   -> writes BASE channels only;
//   - SYSTEM link -> writes BASE and SYSTEM channels;
//   - BOARD link  -> gets the SYSTEM identity (a frame never writes a BOARD
//                    channel).
//
// The target is a copy of the generated channel layout (layers, directions)
// with every value cleared. The TX side always sends the SYSTEM prefix, so the
// frame carries both a BASE channel (THROTTLE_BUS) and a SYSTEM channel
// (RUNLEVEL). Requires the TX↔RX jumper, like Group B.
// =============================================================================

static AnalogComBus  rxTargetAnalog[kNAnalog];
static DigitalComBus rxTargetDigital[kNDigital];
static ComBus        rxTargetBus = {
    .analogBus  = rxTargetAnalog,
    .digitalBus = rxTargetDigital
};

static constexpr uint8_t  kThrottleIdx  = static_cast<uint8_t>(AnalogComBusID::THROTTLE_BUS);   ///< BASE channel
static constexpr uint8_t  kRunLevelIdx  = static_cast<uint8_t>(AnalogComBusID::RUNLEVEL);       ///< SYSTEM channel
static constexpr uint16_t kThrottleVal  = 321u;

/** Clear rxTargetBus: same layout as the generated bus, every value at zero. */
static void resetRxTarget(void) {
    for (uint8_t i = 0u; i < kNAnalog; ++i) {
        rxTargetAnalog[i]       = AnalogComBusArray[i];
        rxTargetAnalog[i].value = 0u;
    }
    for (uint8_t i = 0u; i < kNDigital; ++i) {
        rxTargetDigital[i]              = DigitalComBusArray[i];
        rxTargetDigital[i].value        = false;
        rxTargetDigital[i].localValue   = false;   // both_or state: not inherited from txComBus
        rxTargetDigital[i].localWritten = false;
        rxTargetDigital[i].remoteValue  = false;
    }
    rxTargetBus.analogBusMaxVal = txComBus.analogBusMaxVal;
}

/** Send one SYSTEM-prefix frame and let an RX link of layer `rxLayer` apply it onto rxTargetBus. */
static void sendAndApply(ChanLayer rxLayer) {
    TEST_ASSERT_NOT_NULL_MESSAGE(s_com, "Run test_loopback_init first");
    resetRxTarget();

    combus_tx_init(kLinkIdx, s_com, kCfg, 50u, ChanLayer::SYSTEM);
    combus_rx_init(kLinkIdx, s_com, kCfg, rxAnalogBuf, rxDigitalBuf, &rxTargetBus, rxLayer);

    fillRandom(0x5EEDC0DEul);
    txComBus.analogBus[kThrottleIdx].value = kThrottleVal;
    combus_set_analog(txComBus, AnalogComBusID::RUNLEVEL, (uint16_t)RunLevel::RUNNING, ChanLayer::SYSTEM);

    delay(30u);
    combus_tx_update(&txComBus);
    delay(kRxPollMs);
    combus_rx_update(&txComBus);

    TEST_ASSERT_NOT_NULL_MESSAGE(combus_rx_snapshot(kLinkIdx), "No snapshot — check TX↔RX jumper");
    TEST_ASSERT_TRUE_MESSAGE(combus_rx_snapshot(kLinkIdx)->analog[kThrottleIdx] == kThrottleVal,
                             "Frame not received intact — check TX↔RX jumper");
}

/** SYSTEM link: the SYSTEM channel and the BASE channel are both applied. */
static void test_rx_system_link_applies_system_channels(void) {
    sendAndApply(ChanLayer::SYSTEM);

    TEST_ASSERT_EQUAL_UINT16(kThrottleVal, rxTargetBus.analogBus[kThrottleIdx].value);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)RunLevel::RUNNING, rxTargetBus.analogBus[kRunLevelIdx].value);
}

/** BASE link: the BASE channel is applied, the SYSTEM channel is refused. */
static void test_rx_base_link_refuses_system_channels(void) {
    sendAndApply(ChanLayer::BASE);

    TEST_ASSERT_EQUAL_UINT16(kThrottleVal, rxTargetBus.analogBus[kThrottleIdx].value);
    TEST_ASSERT_EQUAL_UINT16(0u, rxTargetBus.analogBus[kRunLevelIdx].value);
}

/** BOARD link: gets the SYSTEM identity, so both channels are applied. */
static void test_rx_board_link_gets_system_identity(void) {
    sendAndApply(ChanLayer::BOARD);

    TEST_ASSERT_EQUAL_UINT16(kThrottleVal, rxTargetBus.analogBus[kThrottleIdx].value);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)RunLevel::RUNNING, rxTargetBus.analogBus[kRunLevelIdx].value);
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

/**
 * Placeholder transport for the MD5 resolution tests.
 *
 * combus_protocol_init() returns at once on a null `com`, before it resolves
 * the MD5 pointer, so the link needs a non-null transport. It is never called:
 * these links have neither TX nor RX configuration.
 */
static NodeCom s_unusedCom = { nullptr, nullptr, nullptr, nullptr, "test_unused" };

/** combus_protocol_init() with BASE layer must resolve to kCombusBaseComBusMd5. */
static void test_md5_resolution_base(void) {
    static CombusHandshakeContext ctx = {};
    static ComBusLink link = {};

    link.name        = "test_remote";
    link.com         = &s_unusedCom;  // non-null: combus_protocol_init() rejects a null transport
    link.txCfg       = std::nullopt;  // skip TX init
    link.rxCfg       = std::nullopt;  // skip RX init
    link.handshakeCtx = &ctx;
    link.layer       = ChanLayer::BASE;

    // The MD5 resolution happens BEFORE the TX/RX init, so the transport
    // is never used — the switch on link->layer runs first.
    combus_protocol_init(kLinkIdx, &link);

    // Content, not address: the generated MD5 arrays are `static constexpr`, so each
    // translation unit has its own copy at a different address.
    TEST_ASSERT_NOT_NULL(ctx.expectedMd5);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(combus::wire::kCombusBaseComBusMd5, ctx.expectedMd5, 16u);
}

/** combus_protocol_init() with SYSTEM layer must resolve to kCombusSystemComBusMd5. */
static void test_md5_resolution_system(void) {
    static CombusHandshakeContext ctx = {};
    static ComBusLink link = {};

    link.name        = "test_local";
    link.com         = &s_unusedCom;
    link.txCfg       = std::nullopt;
    link.rxCfg       = std::nullopt;
    link.handshakeCtx = &ctx;
    link.layer       = ChanLayer::SYSTEM;

    combus_protocol_init(kLinkIdx, &link);

    TEST_ASSERT_NOT_NULL(ctx.expectedMd5);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(combus::wire::kCombusSystemComBusMd5, ctx.expectedMd5, 16u);
}

/** combus_protocol_init() with BOARD layer must resolve to kCombusComBusMd5 (FULL). */
static void test_md5_resolution_board(void) {
    static CombusHandshakeContext ctx = {};
    static ComBusLink link = {};

    link.name        = "test_system";
    link.com         = &s_unusedCom;
    link.txCfg       = std::nullopt;
    link.rxCfg       = std::nullopt;
    link.handshakeCtx = &ctx;
    link.layer       = ChanLayer::BOARD;

    combus_protocol_init(kLinkIdx, &link);

    TEST_ASSERT_NOT_NULL(ctx.expectedMd5);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(combus::wire::kCombusComBusMd5, ctx.expectedMd5, 16u);
}

// =============================================================================
// GROUP I — LINKS FROM THEIR DESCRIPTION (chantier 0.4)
//
// combus_link_build() fills a ComBusLink from a ComDevMap without opening any
// port, so these tests need no wiring. s_unusedCom (Group E) stands for the
// transport that combus_links_init() would have opened.
// =============================================================================

static constexpr ComProtocolCfg kTestProtocol = { 115200u, 50u, 250u };

static constexpr ComDev kTestDevUni = {
    "test_uni", LinkScope::INTER_NODE, LinkType::UNIDIRECTIONAL,
    &kTestProtocol, LinkActivation::ALWAYS, nullptr
};
static constexpr ComDev kTestDevBi = {
    "test_bi", LinkScope::INTRA_NODE, LinkType::BIDIRECTIONAL,
    &kTestProtocol, LinkActivation::ALWAYS, nullptr
};

static const ComPort s_testPort = ComPort::makeUart("test", { 2, kTxPin, kRxPin, 115200u });

static ComLinkRuntime s_testRuntime;

/** One-way INTER_NODE link wired TX: BASE layer, BASE frame size, TX only. */
static void test_link_build_inter_node_tx(void) {
    const ComDevMap map = { &kTestDevUni, &s_testPort, LinkEnd::TX };
    ComBusLink link{};

    TEST_ASSERT_TRUE(combus_link_build(map, &s_unusedCom, s_testRuntime, link));

    TEST_ASSERT_TRUE(link.layer == ChanLayer::BASE);
    TEST_ASSERT_TRUE(link.com == &s_unusedCom);
    TEST_ASSERT_TRUE(link.handshakeCtx == &s_testRuntime.handshake);
    TEST_ASSERT_EQUAL_STRING("test_uni", link.name);
    TEST_ASSERT_TRUE(link.txCfg.has_value());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT),  link.txCfg->nAnalog);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT), link.txCfg->nDigital);
    TEST_ASSERT_EQUAL_UINT32(50u, link.txHz);
    TEST_ASSERT_FALSE(link.rxCfg.has_value());
}

/** One-way link wired RX: RX only, with the runtime buffers. */
static void test_link_build_rx_only(void) {
    const ComDevMap map = { &kTestDevUni, &s_testPort, LinkEnd::RX };
    ComBusLink link{};

    TEST_ASSERT_TRUE(combus_link_build(map, &s_unusedCom, s_testRuntime, link));

    TEST_ASSERT_FALSE(link.txCfg.has_value());
    TEST_ASSERT_TRUE(link.rxCfg.has_value());
    TEST_ASSERT_TRUE(link.analogBuf  == s_testRuntime.analogRx);
    TEST_ASSERT_TRUE(link.digitalBuf == s_testRuntime.digitalRx);
}

/** Two-way INTRA_NODE link wired TX_RX: SYSTEM layer, SYSTEM frame size, TX and RX. */
static void test_link_build_intra_node_tx_rx(void) {
    const ComDevMap map = { &kTestDevBi, &s_testPort, LinkEnd::TX_RX };
    ComBusLink link{};

    TEST_ASSERT_TRUE(combus_link_build(map, &s_unusedCom, s_testRuntime, link));

    TEST_ASSERT_TRUE(link.layer == ChanLayer::SYSTEM);
    TEST_ASSERT_TRUE(link.txCfg.has_value());
    TEST_ASSERT_TRUE(link.rxCfg.has_value());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT),  link.txCfg->nAnalog);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT), link.rxCfg->nDigital);
}

/** The scope-to-layer conversion is the single bridge to ChanLayer. */
static void test_link_scope_layer(void) {
    TEST_ASSERT_TRUE(linkScopeLayer(LinkScope::INTER_NODE) == ChanLayer::BASE);
    TEST_ASSERT_TRUE(linkScopeLayer(LinkScope::INTRA_NODE) == ChanLayer::SYSTEM);
}

/** Wirings that must be refused, and a descriptor left untouched on refusal. */
static void test_link_build_refuses_bad_wiring(void) {
    ComBusLink link{};
    link.name = "untouched";

    // Bidirectional link not wired TX_RX.
    const ComDevMap biOnTx = { &kTestDevBi, &s_testPort, LinkEnd::TX };
    TEST_ASSERT_FALSE(combus_link_check(biOnTx));

    // Unidirectional link wired TX_RX.
    const ComDevMap uniOnTxRx = { &kTestDevUni, &s_testPort, LinkEnd::TX_RX };
    TEST_ASSERT_FALSE(combus_link_check(uniOnTxRx));

    // Activation other than ALWAYS (not supported yet).
    static constexpr ComDev coupled = {
        "test_coupled", LinkScope::INTER_NODE, LinkType::UNIDIRECTIONAL,
        &kTestProtocol, LinkActivation::ON_COUPLING, nullptr
    };
    const ComDevMap onCoupling = { &coupled, &s_testPort, LinkEnd::TX };
    TEST_ASSERT_FALSE(combus_link_check(onCoupling));

    // Line rate above the port ceiling.
    static constexpr ComProtocolCfg tooFast = { 921600u, 50u, 250u };
    static constexpr ComDev fast = {
        "test_fast", LinkScope::INTER_NODE, LinkType::UNIDIRECTIONAL,
        &tooFast, LinkActivation::ALWAYS, nullptr
    };
    const ComDevMap overRate = { &fast, &s_testPort, LinkEnd::TX };
    TEST_ASSERT_FALSE(combus_link_check(overRate));

    // Transmitting end without a transmit rate.
    static constexpr ComProtocolCfg noHz = { 115200u, 0u, 250u };
    static constexpr ComDev silent = {
        "test_silent", LinkScope::INTER_NODE, LinkType::UNIDIRECTIONAL,
        &noHz, LinkActivation::ALWAYS, nullptr
    };
    const ComDevMap noTxHz = { &silent, &s_testPort, LinkEnd::TX };
    TEST_ASSERT_FALSE(combus_link_check(noTxHz));

    // Receiving end without a link timeout.
    static constexpr ComProtocolCfg noTimeout = { 115200u, 50u, 0u };
    static constexpr ComDev deaf = {
        "test_deaf", LinkScope::INTER_NODE, LinkType::UNIDIRECTIONAL,
        &noTimeout, LinkActivation::ALWAYS, nullptr
    };
    const ComDevMap noLinkTimeout = { &deaf, &s_testPort, LinkEnd::RX };
    TEST_ASSERT_FALSE(combus_link_check(noLinkTimeout));

    // Null transport: combus_link_build() refuses and does not write the descriptor.
    const ComDevMap fine = { &kTestDevUni, &s_testPort, LinkEnd::TX };
    TEST_ASSERT_FALSE(combus_link_build(fine, nullptr, s_testRuntime, link));
    TEST_ASSERT_EQUAL_STRING("untouched", link.name);
}


// =============================================================================
// GROUP J — RECEIVING LINK HEALTH (chantier 1.1)
//
// A TX↔RX link is built from its description on the loopback port, with
// rxTargetBus (Group H) as the node ComBus. The link is lost until its first
// frame and after linkTimeoutMs of silence. Requires the TX↔RX jumper.
// =============================================================================

static constexpr ComProtocolCfg kHealthProtocol = { 115200u, 50u, 100u, 200u };   ///< lost after 100 ms of silence, 200 ms startup grace

static constexpr ComDev kHealthDev = {
    "test_health", LinkScope::INTRA_NODE, LinkType::BIDIRECTIONAL,
    &kHealthProtocol, LinkActivation::ALWAYS, nullptr
};

static ComLinkRuntime s_healthRuntime;
static ComBusLink     s_healthLink;

/** Build the health link on the loopback port, apply its frames to rxTargetBus and put it under watch. */
static void startHealthLink(LinkEnd end, const ComDev& dev) {
    TEST_ASSERT_NOT_NULL_MESSAGE(s_com, "Run test_loopback_init first");
    resetRxTarget();

    const ComDevMap map = { &dev, &s_testPort, end };
    TEST_ASSERT_TRUE(combus_link_build(map, s_com, s_healthRuntime, s_healthLink));
    if (s_healthLink.rxCfg.has_value()) { s_healthLink.target = &rxTargetBus; }

    combus_protocol_init(kLinkIdx, &s_healthLink);
    combus_links_watch(&s_healthRuntime, 1u);
}

/** Run the handshake on the loopback until the contract is STABLE (request, then its ACK). */
static void ensureStable(void) {
    CombusHandshakeContext* ctx = &s_healthRuntime.handshake;
    const uint32_t t0 = millis();
    while (!combus_handshake_is_contract_validated(ctx) && (millis() - t0) < 1500u) {
        combus_handshake_tx_update(ctx, s_com);
        delay(10u);
        combus_rx_update(&rxTargetBus);
    }
    TEST_ASSERT_TRUE_MESSAGE(combus_handshake_is_contract_validated(ctx), "link not established — check TX↔RX jumper");
}

/** Establish the link if needed, send one frame from txComBus and let com_rx_update() receive it. */
static void sendOneFrameAndReceive(void) {
    ensureStable();
    fillRandom(0x4EA17Bul);
    delay(30u);
    combus_tx_update(&txComBus);
    delay(kRxPollMs);
    com_rx_update(rxTargetBus);
}

/** Nothing received yet: the link is lost (failsafe by default). */
static void test_link_health_lost_until_first_frame(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

    com_rx_update(rxTargetBus);

    TEST_ASSERT_TRUE(combus_links_rx_lost());
}

/** A frame arrives: the link is alive and its channels are applied. */
static void test_link_health_alive_after_frame(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

    sendOneFrameAndReceive();

    TEST_ASSERT_FALSE(combus_links_rx_lost());
    TEST_ASSERT_EQUAL_UINT16(txComBus.analogBus[kThrottleIdx].value, rxTargetBus.analogBus[kThrottleIdx].value);
}

/** The peer goes silent for longer than linkTimeoutMs: the link is lost again. */
static void test_link_health_lost_after_silence(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    sendOneFrameAndReceive();
    TEST_ASSERT_FALSE(combus_links_rx_lost());

    delay(kHealthProtocol.linkTimeoutMs + 60u);
    com_rx_update(rxTargetBus);

    TEST_ASSERT_TRUE(combus_links_rx_lost());
}

/** A TX-only link cannot know its peer: it never counts as lost. */
static void test_link_health_ignores_tx_only_link(void) {
    startHealthLink(LinkEnd::TX, kTestDevUni);

    com_rx_update(rxTargetBus);

    TEST_ASSERT_FALSE(combus_links_rx_lost());

    combus_links_watch(nullptr, 0u);
}


// =============================================================================
// GROUP K — DENIED WRITES ARE COUNTED (chantier 1.1)
//
// COM_LINK_LOST is a BOARD-scope channel: a SYSTEM caller is refused. This is
// the mistake that kept REMOTE_LINK_LOST stuck on the Volvo; the count makes
// it visible (it is also shown on the dashboard).
// =============================================================================

/** A refused write is counted each time, an accepted one is not. */
static void test_denied_write_is_counted(void) {
    const uint32_t before = combus_denied_writes();

    TEST_ASSERT_FALSE(combus_set_digital(txComBus, DigitalComBusID::COM_LINK_LOST, true, ChanLayer::SYSTEM));
    TEST_ASSERT_FALSE(combus_set_digital(txComBus, DigitalComBusID::COM_LINK_LOST, true, ChanLayer::SYSTEM));
    TEST_ASSERT_EQUAL_UINT32(before + 2u, combus_denied_writes());

    TEST_ASSERT_TRUE(combus_set_digital(txComBus, DigitalComBusID::COM_LINK_LOST, false, ChanLayer::BOARD));
    TEST_ASSERT_EQUAL_UINT32(before + 2u, combus_denied_writes());
}


// =============================================================================
// GROUP L — BOTH_OR MERGE, ANTI PING-PONG (chantier 1.1b)
//
// The health link of Group J (TX↔RX on the loopback port, frames applied to
// rxTargetBus) receives frames built from peerBus, as a remote board would send
// them. FAILSAFE is a both_or channel: value = this board's value OR the live
// links, a frame never overwrites it, and what is sent on a link never echoes
// what that link sent (see src/core/system/combus/protocol/both_or.md).
// =============================================================================

static AnalogComBus  peerAnalog[kNAnalog];
static DigitalComBus peerDigital[kNDigital];
static ComBus        peerBus = {
    .analogBus  = peerAnalog,
    .digitalBus = peerDigital
};

static constexpr uint8_t kFailsafeIdx = static_cast<uint8_t>(DigitalComBusID::FAILSAFE);

/** Write on the wire one frame as a remote peer would send it, with FAILSAFE as given. */
static void sendPeerFrame(bool failsafe) {
    for (uint8_t i = 0u; i < kNAnalog; ++i)  { peerAnalog[i]  = AnalogComBusArray[i]; }
    for (uint8_t i = 0u; i < kNDigital; ++i) { peerDigital[i] = DigitalComBusArray[i]; peerDigital[i].value = false; }
    peerBus.analogBusMaxVal         = txComBus.analogBusMaxVal;
    peerDigital[kFailsafeIdx].value = failsafe;

    const ComBusFrameCfg sysCfg = {
        static_cast<uint8_t>(AnalogComBusSystemID::CH_COUNT),
        static_cast<uint8_t>(DigitalComBusSystemID::CH_COUNT)
    };
    static uint8_t seq = 0u;
    seq = (seq == 255u) ? 1u : (uint8_t)(seq + 1u);

    uint8_t buf[CombusRxBufSize];
    const uint8_t len = combus_frame_encode(sysCfg, buf, &peerBus, seq, sysCfg.nAnalog, sysCfg.nDigital);
    TEST_ASSERT_TRUE_MESSAGE(len > 0u, "peer frame not encoded");

    s_com->write(s_com->ctx, buf, len);
    delay(kRxPollMs);
}

static bool failsafeOnBus(void) {
    return rxTargetBus.digitalBus[kFailsafeIdx].value;
}

/** A peer in fault makes the merged value true while this board's own value is false. */
static void test_both_or_merges_peer_value(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    ensureStable();
    combus_set_digital(rxTargetBus, DigitalComBusID::FAILSAFE, false);   // this board: no fault

    sendPeerFrame(true);
    com_rx_update(rxTargetBus);

    TEST_ASSERT_TRUE(failsafeOnBus());
    TEST_ASSERT_FALSE(rxTargetBus.digitalBus[kFailsafeIdx].localValue);   // the local value is kept apart
}

/** The peer clears its fault: the merged value falls back to false. */
static void test_both_or_falls_back_when_peer_clears(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    ensureStable();
    combus_set_digital(rxTargetBus, DigitalComBusID::FAILSAFE, false);

    sendPeerFrame(true);
    com_rx_update(rxTargetBus);
    TEST_ASSERT_TRUE(failsafeOnBus());

    sendPeerFrame(false);
    com_rx_update(rxTargetBus);
    TEST_ASSERT_FALSE(failsafeOnBus());
}

/** The peer goes silent: its last value is stale and no longer counts. */
static void test_both_or_ignores_silent_peer(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    ensureStable();
    combus_set_digital(rxTargetBus, DigitalComBusID::FAILSAFE, false);

    sendPeerFrame(true);
    com_rx_update(rxTargetBus);
    TEST_ASSERT_TRUE(failsafeOnBus());

    delay(kHealthProtocol.linkTimeoutMs + 60u);
    com_rx_update(rxTargetBus);
    TEST_ASSERT_FALSE(failsafeOnBus());
}

/** The frame sent back on the link does not echo the peer's value (anti ping-pong). */
static void test_both_or_sends_no_echo(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    ensureStable();
    combus_set_digital(rxTargetBus, DigitalComBusID::FAILSAFE, false);

    sendPeerFrame(true);
    com_rx_update(rxTargetBus);
    TEST_ASSERT_TRUE(failsafeOnBus());                  // merged: true

    delay(30u);
    combus_tx_update(&rxTargetBus);                     // loops back on the same link
    delay(kRxPollMs);
    combus_rx_update(&rxTargetBus);

    const ComBusFrame* snap = combus_rx_snapshot(kLinkIdx);
    TEST_ASSERT_NOT_NULL(snap);
    TEST_ASSERT_FALSE_MESSAGE(snap->digital[kFailsafeIdx], "the peer's FAILSAFE was echoed back to it");
}

/** A board that never wrote the channel does not contribute its default value. */
static void test_both_or_board_never_writing_does_not_contribute(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

        // A diffused copy never written by this board: still at its default value, high (fault)
    rxTargetBus.digitalBus[kFailsafeIdx].value = true;
    TEST_ASSERT_FALSE(rxTargetBus.digitalBus[kFailsafeIdx].localWritten);

    com_rx_update(rxTargetBus);                       // no peer frame, never written locally

    TEST_ASSERT_FALSE(failsafeOnBus());

    combus_links_watch(nullptr, 0u);
}


// =============================================================================
// GROUP M — LINK LOSS POLICY (chantier 1.1c)
//
// One test per row of the table validated by the owner (rows 1 to 11; rows 12
// to 14 need several links or two boards and rely on the failsafe priority of
// the runlevel chain and on the both_or merge, tested in Group L).  The link is
// built on the loopback port (Group J helpers); its startup grace is 200 ms and
// its timeout 100 ms (kHealthProtocol) instead of 3 s and 250 ms.
// =============================================================================

static constexpr ComDev kInterDev = {             ///< inter-node, default onLoss -> IDLE
    "test_inter", LinkScope::INTER_NODE, LinkType::BIDIRECTIONAL,
    &kHealthProtocol, LinkActivation::ALWAYS, nullptr
};
static constexpr ComDev kNoneDev = {              ///< intra-node, onLoss NONE (a sound board)
    "test_none", LinkScope::INTRA_NODE, LinkType::BIDIRECTIONAL,
    &kHealthProtocol, LinkActivation::ALWAYS, nullptr, LinkLossAction::NONE
};
static constexpr ComDev kInterFailsafeDev = {     ///< inter-node, onLoss FAILSAFE imposed
    "test_inter_fs", LinkScope::INTER_NODE, LinkType::BIDIRECTIONAL,
    &kHealthProtocol, LinkActivation::ALWAYS, nullptr, LinkLossAction::FAILSAFE
};

static bool comLinkLostOnBus(void) {
    return rxTargetBus.digitalBus[static_cast<uint8_t>(DigitalComBusID::COM_LINK_LOST)].value;
}

static bool failsafeComLinkOnBus(void) {
    return rxTargetBus.digitalBus[static_cast<uint8_t>(DigitalComBusID::FAILSAFE_COM_LINK)].value;
}

/** Default onLoss by scope: intra-node -> FAILSAFE, inter-node -> IDLE; an explicit value is kept. */
static void test_loss_action_by_scope(void) {
    TEST_ASSERT_TRUE(linkLossAction(kHealthDev)        == LinkLossAction::FAILSAFE);
    TEST_ASSERT_TRUE(linkLossAction(kInterDev)         == LinkLossAction::IDLE);
    TEST_ASSERT_TRUE(linkLossAction(kNoneDev)          == LinkLossAction::NONE);
    TEST_ASSERT_TRUE(linkLossAction(kInterFailsafeDev) == LinkLossAction::FAILSAFE);
}

/** Row 1: intra-node link, no frame yet, within the grace: no effect. */
static void test_loss_row1_failsafe_link_in_grace(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    com_rx_update(rxTargetBus);

    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::NEVER_SEEN);
    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::FAILSAFE));
    TEST_ASSERT_FALSE(failsafeComLinkOnBus());
}

/** Row 2: intra-node link, still no frame after the grace: FAILSAFE. */
static void test_loss_row2_failsafe_link_never_seen_after_grace(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    delay(kHealthProtocol.startupGraceMs + 60u);
    com_rx_update(rxTargetBus);

    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::NEVER_SEEN);
    TEST_ASSERT_TRUE(combus_links_lost(LinkLossAction::FAILSAFE));
    TEST_ASSERT_TRUE(failsafeComLinkOnBus());
    TEST_ASSERT_FALSE(comLinkLostOnBus());
}

/** Rows 3, 4, 5: intra-node link alive, then lost (FAILSAFE), then back (contribution drops). */
static void test_loss_rows3to5_failsafe_link_alive_lost_back(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

    sendOneFrameAndReceive();                                      // row 3
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::ALIVE);
    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::FAILSAFE));
    TEST_ASSERT_FALSE(failsafeComLinkOnBus());

    delay(kHealthProtocol.linkTimeoutMs + 60u);                    // row 4
    com_rx_update(rxTargetBus);
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::LOST);
    TEST_ASSERT_TRUE(combus_links_lost(LinkLossAction::FAILSAFE));
    TEST_ASSERT_TRUE(failsafeComLinkOnBus());

    sendOneFrameAndReceive();                                      // row 5
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::ALIVE);
    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::FAILSAFE));
    TEST_ASSERT_FALSE(failsafeComLinkOnBus());
}

/** Row 6: inter-node link, no frame yet: IDLE at once (no grace), never FAILSAFE. */
static void test_loss_row6_idle_link_never_seen(void) {
    startHealthLink(LinkEnd::TX_RX, kInterDev);
    com_rx_update(rxTargetBus);

    TEST_ASSERT_TRUE(combus_links_lost(LinkLossAction::IDLE));
    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::FAILSAFE));
    TEST_ASSERT_TRUE(comLinkLostOnBus());
    TEST_ASSERT_FALSE(failsafeComLinkOnBus());
}

/** Rows 7, 8: inter-node link alive, then lost (IDLE), then back (contribution drops). */
static void test_loss_rows7to8_idle_link_lost_back(void) {
    startHealthLink(LinkEnd::TX_RX, kInterDev);

    sendOneFrameAndReceive();
    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::IDLE));
    TEST_ASSERT_FALSE(comLinkLostOnBus());

    delay(kHealthProtocol.linkTimeoutMs + 60u);                    // row 7
    com_rx_update(rxTargetBus);
    TEST_ASSERT_TRUE(combus_links_lost(LinkLossAction::IDLE));
    TEST_ASSERT_TRUE(comLinkLostOnBus());

    sendOneFrameAndReceive();                                      // row 8
    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::IDLE));
    TEST_ASSERT_FALSE(comLinkLostOnBus());
}

/** Row 9: onLoss NONE: never contributes, even never seen after the grace. */
static void test_loss_row9_none_link_never_contributes(void) {
    startHealthLink(LinkEnd::TX_RX, kNoneDev);
    delay(kHealthProtocol.startupGraceMs + 60u);
    com_rx_update(rxTargetBus);

    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::IDLE));
    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::FAILSAFE));
    TEST_ASSERT_FALSE(comLinkLostOnBus());
    TEST_ASSERT_FALSE(failsafeComLinkOnBus());
}

/** Row 10: inter-node link with onLoss FAILSAFE imposed: a loss is a FAILSAFE, not an IDLE. */
static void test_loss_row10_inter_link_failsafe_imposed(void) {
    startHealthLink(LinkEnd::TX_RX, kInterFailsafeDev);
    sendOneFrameAndReceive();

    delay(kHealthProtocol.linkTimeoutMs + 60u);
    com_rx_update(rxTargetBus);
    TEST_ASSERT_TRUE(combus_links_lost(LinkLossAction::FAILSAFE));
    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::IDLE));
}

/** Row 11: a TX-only link is not watched: it never contributes. */
static void test_loss_row11_tx_only_link_never_contributes(void) {
    startHealthLink(LinkEnd::TX, kTestDevUni);
    delay(kHealthProtocol.startupGraceMs + 60u);
    com_rx_update(rxTargetBus);

    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::IDLE));
    TEST_ASSERT_FALSE(combus_links_lost(LinkLossAction::FAILSAFE));

    combus_links_watch(nullptr, 0u);
}


// =============================================================================
// GROUP N — LINK CONTRACT, ACK AND BLOCKING (chantier 1.2)
//
// One test per row of the table validated by the owner.  The link is built on
// the loopback port (Group J helpers).  What a remote peer would send is forged
// on the wire (handshake or data frame); what this board sends comes back on
// its own RX, so a request of its own is answered by itself.
// =============================================================================

static constexpr ComDev kRxOnlyDev = {            ///< one-way intra-node link, wired RX here
    "test_rx_only", LinkScope::INTRA_NODE, LinkType::UNIDIRECTIONAL,
    &kHealthProtocol, LinkActivation::ALWAYS, nullptr
};

static CombusHandshakeContext* linkCtx(void) { return &s_healthRuntime.handshake; }

static bool isStable(void) { return combus_handshake_is_contract_validated(linkCtx()); }

/** Throw away every byte waiting on the loopback RX. */
static void drainWire(void) {
    while (s_com->available(s_com->ctx) > 0) { (void)s_com->readByte(s_com->ctx); }
}

/** Read everything waiting on the loopback RX: true if it is only whole handshake frames. */
static bool wireHasOnlyHandshakes(void) {
    uint8_t buf[CombusRxBufSize];
    uint16_t n = 0u;
    while (s_com->available(s_com->ctx) > 0 && n < sizeof(buf)) {
        buf[n++] = (uint8_t)s_com->readByte(s_com->ctx);
    }
    if ((n % CombusFrameHandshakeMinLen) != 0u) { return false; }
    for (uint16_t i = 0u; i < n; i += CombusFrameHandshakeMinLen) {
        if (buf[i] != CombusFrameSof || buf[i + 3u] != 0u) { return false; }   // seq 0 = handshake
    }
    return true;
}

/** Write on the wire a handshake as a remote peer would send it. */
static void sendForgedHandshake(bool ack, bool goodMd5, uint8_t major, uint8_t minor) {
    uint8_t frame[CombusFrameHandshakeMinLen];
    const uint8_t payload = sizeof(CombusFrameSof) + sizeof(CombusFrameHeader);

    frame[0] = CombusFrameSof;
    frame[1] = 0u;                                          // nAnalog
    frame[2] = 0u;                                          // nDigital
    frame[3] = 0u;                                          // seq 0 = handshake
    frame[4] = ack ? kCombusHandshakeFlagAck : 0u;          // flags
    for (uint8_t i = 0u; i < 16u; ++i) { frame[payload + i] = linkCtx()->expectedMd5[i]; }
    if (!goodMd5) { frame[payload] ^= 0xFFu; }
    frame[payload + 16u] = major;
    frame[payload + 17u] = minor;
    frame[payload + 18u] = combus_frame_crc8(frame, (uint8_t)(payload + 18u));

    s_com->write(s_com->ctx, frame, sizeof(frame));
    delay(kRxPollMs);
}

static void sendGoodAck(void) {
    sendForgedHandshake(true, true, combus::wire::kProjectVersionMajor, combus::wire::kProjectVersionMinor);
}

/** Let the RX module read what is on the wire. */
static void pumpRx(void) { combus_rx_update(&rxTargetBus); }

/** Row 1: both ends start together: request, ACK, STABLE; the periodic ACK keeps it STABLE. */
static void test_contract_row1_request_ack_stable(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    TEST_ASSERT_FALSE(isStable());

    for (uint32_t t0 = millis(); (millis() - t0) < 300u; ) {
        combus_tx_update(&txComBus);
        delay(10u);
        pumpRx();
    }
    TEST_ASSERT_TRUE(isStable());
    TEST_ASSERT_NOT_NULL(combus_rx_snapshot(kLinkIdx));             // the data flows

    for (uint32_t t0 = millis(); (millis() - t0) < 3500u; ) {       // longer than 3 missed ACKs
        combus_tx_update(&txComBus);
        delay(10u);
        pumpRx();
    }
    TEST_ASSERT_TRUE(isStable());
}

/** Row 2: no answer: only requests go out, never data; the peer that shows up answers. */
static void test_contract_row2_peer_late(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

    for (uint32_t t0 = millis(); (millis() - t0) < 1000u; ) {       // burst then requests, nobody answers
        combus_tx_update(&txComBus);
        delay(20u);
        TEST_ASSERT_TRUE_MESSAGE(wireHasOnlyHandshakes(), "data sent while the contract is UNKNOWN");
    }
    TEST_ASSERT_FALSE(isStable());

    ensureStable();                                                 // the next request is answered
}

/** Row 3: other MD5: refused, a request is still answered, data frames are not applied. */
static void test_contract_row3_md5_mismatch(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

    sendForgedHandshake(false, false, combus::wire::kProjectVersionMajor, combus::wire::kProjectVersionMinor);
    pumpRx();
    TEST_ASSERT_FALSE(isStable());
    TEST_ASSERT_TRUE(linkCtx()->ackPending);

    sendForgedHandshake(true, false, combus::wire::kProjectVersionMajor, combus::wire::kProjectVersionMinor);
    pumpRx();
    TEST_ASSERT_FALSE(isStable());

    sendPeerFrame(false);
    pumpRx();
    TEST_ASSERT_NULL(combus_rx_snapshot(kLinkIdx));
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::NEVER_SEEN);
}

/** Row 4: same MD5 and major, other minor: STABLE. */
static void test_contract_row4_minor_differs(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

    sendForgedHandshake(true, true, combus::wire::kProjectVersionMajor,
                        (uint8_t)(combus::wire::kProjectVersionMinor + 1u));
    pumpRx();
    TEST_ASSERT_TRUE(isStable());
}

/** Row 5: same MD5, other major: refused. */
static void test_contract_row5_major_differs(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

    sendForgedHandshake(true, true, (uint8_t)(combus::wire::kProjectVersionMajor + 1u),
                        combus::wire::kProjectVersionMinor);
    pumpRx();
    TEST_ASSERT_FALSE(isStable());
}

/** Row 6: data before the contract is not applied and the link is not alive; after the ACK it is. */
static void test_contract_row6_data_ignored_until_stable(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

    sendPeerFrame(false);
    pumpRx();
    TEST_ASSERT_NULL(combus_rx_snapshot(kLinkIdx));
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::NEVER_SEEN);

    sendGoodAck();
    pumpRx();
    TEST_ASSERT_TRUE(isStable());

    sendPeerFrame(false);
    pumpRx();
    TEST_ASSERT_NOT_NULL(combus_rx_snapshot(kLinkIdx));
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::ALIVE);
}

/** Row 7: silence drops the contract; the data that comes back waits for a new handshake. */
static void test_contract_row7_silence_then_renegotiation(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    sendGoodAck();
    pumpRx();
    sendPeerFrame(false);
    pumpRx();
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::ALIVE);

    delay(kHealthProtocol.linkTimeoutMs + 60u);
    pumpRx();
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::LOST);
    TEST_ASSERT_FALSE(isStable());

    sendPeerFrame(false);                                           // the peer is back, data first
    pumpRx();
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::LOST);

    sendGoodAck();
    pumpRx();
    sendPeerFrame(false);
    pumpRx();
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::ALIVE);
}

/** Row 8: STABLE, a handshake with another MD5 arrives: back to UNKNOWN. */
static void test_contract_row8_bad_handshake_while_stable(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    sendGoodAck();
    pumpRx();
    TEST_ASSERT_TRUE(isStable());

    sendForgedHandshake(false, false, combus::wire::kProjectVersionMajor, combus::wire::kProjectVersionMinor);
    pumpRx();
    TEST_ASSERT_FALSE(isStable());
}

/** Row 9: data still arrives but no ACK for 1 s: the peer no longer receives me, the link is lost. */
static void test_contract_row9_no_ack_means_lost(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    sendGoodAck();
    pumpRx();

    bool stableAt600ms = false;
    for (uint32_t t0 = millis(); (millis() - t0) < 1300u; ) {
        sendPeerFrame(false);                                       // data, never an ACK
        pumpRx();
        if ((millis() - t0) > 500u && (millis() - t0) < 700u) { stableAt600ms = isStable(); }
    }
    TEST_ASSERT_TRUE_MESSAGE(stableAt600ms, "dropped too early");
    TEST_ASSERT_FALSE_MESSAGE(isStable(), "still STABLE after more than 1 s without ACK");
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::LOST);
}

/** Row 10: a receive-only end ignores data until the request of its peer, never answers it. */
static void test_contract_row10_rx_only_end(void) {
    startHealthLink(LinkEnd::RX, kRxOnlyDev);

    sendPeerFrame(false);
    pumpRx();
    TEST_ASSERT_NULL(combus_rx_snapshot(kLinkIdx));

    sendForgedHandshake(false, true, combus::wire::kProjectVersionMajor, combus::wire::kProjectVersionMinor);
    pumpRx();
    TEST_ASSERT_TRUE(isStable());
    TEST_ASSERT_FALSE(linkCtx()->ackPending);

    sendPeerFrame(false);
    pumpRx();
    TEST_ASSERT_NOT_NULL(combus_rx_snapshot(kLinkIdx));
}

/** Row 11: a transmit-only end sends its data at once and a request every period. */
static void test_contract_row11_tx_only_end(void) {
    startHealthLink(LinkEnd::TX, kTestDevUni);
    drainWire();

    combus_tx_update(&txComBus);                                    // burst request + data
    delay(kRxPollMs);
    drainWire();

    delay(30u);                                                     // data due, no request due
    combus_tx_update(&txComBus);
    delay(kRxPollMs);
    TEST_ASSERT_EQUAL_INT(frameLen(static_cast<uint8_t>(AnalogComBusBaseID::CH_COUNT),
                                   static_cast<uint8_t>(DigitalComBusBaseID::CH_COUNT)),
                          s_com->available(s_com->ctx));
    drainWire();

    for (uint32_t t0 = millis(); (millis() - t0) < 300u; ) {        // end of the burst
        combus_handshake_tx_update(linkCtx(), s_com);
        delay(20u);
    }
    drainWire();

    for (uint32_t t0 = millis(); (millis() - t0) < kCombusHandshakePeriodMs + 50u; ) {   // the next request
        combus_handshake_tx_update(linkCtx(), s_com);
        delay(10u);
        if (s_com->available(s_com->ctx) > 0) { break; }
    }
    delay(kRxPollMs);
    TEST_ASSERT_EQUAL_INT(CombusFrameHandshakeMinLen, s_com->available(s_com->ctx));
    drainWire();

    combus_handshake_tx_update(linkCtx(), s_com);                   // at once: not due yet
    delay(kRxPollMs);
    TEST_ASSERT_EQUAL_INT(0, s_com->available(s_com->ctx));

    combus_links_watch(nullptr, 0u);
}

/** Row 12: a request is answered, an ACK never is. */
static void test_contract_row12_no_reply_to_ack(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);

    sendGoodAck();
    pumpRx();
    TEST_ASSERT_FALSE(linkCtx()->ackPending);

    sendForgedHandshake(false, true, combus::wire::kProjectVersionMajor, combus::wire::kProjectVersionMinor);
    pumpRx();
    TEST_ASSERT_TRUE(linkCtx()->ackPending);
}

/** P4 removed: an active FAILSAFE does not drop the contract. */
static void test_contract_failsafe_keeps_contract(void) {
    startHealthLink(LinkEnd::TX_RX, kHealthDev);
    sendGoodAck();
    pumpRx();
    TEST_ASSERT_TRUE(isStable());

    combus_set_digital(rxTargetBus, DigitalComBusID::FAILSAFE, true);
    pumpRx();
    TEST_ASSERT_TRUE(isStable());
}


// Chantier 12.6 (cleanup): Group F (FS1 failsafe re-arm) REMOVED — link
// health is now monitored by the per-backend *_LINK_LOST contributors
// (PS4_DS4_BT_LINK_LOST, COM_LINK_LOST, ...) aggregated into
// REMOTE_LINK_LOST, which feeds the top-level FAILSAFE aggregator
// (see failsafe_module.md §12.5 new design).
//
// The replacement tests (link health via REMOTE_LINK_LOST aggregation)
// will be added in a future chantier once the contract is finalised.

/** combus_handshake_compare() must match when given the right local MD5. */
static void test_md5_compare_match_with_correct_local(void) {
    // Wire MD5 = BASE view (simulating a peer that speaks BASE).
    const uint8_t* wireMd5 = combus::wire::kCombusBaseComBusMd5;

    // Compare against the BASE local MD5 — must match.
    bool match = combus_handshake_compare(
        combus::wire::kCombusBaseComBusMd5,
        wireMd5,
        combus::wire::kProjectVersionMajor);
    TEST_ASSERT_TRUE_MESSAGE(match, "BASE local MD5 must match BASE wire MD5");

    // Compare against the FULL local MD5 — must NOT match (different views).
    match = combus_handshake_compare(
        combus::wire::kCombusComBusMd5,
        wireMd5,
        combus::wire::kProjectVersionMajor);
    TEST_ASSERT_FALSE_MESSAGE(match, "FULL local MD5 must NOT match BASE wire MD5");

    // Compare against the SYSTEM local MD5 — must NOT match (different views).
    match = combus_handshake_compare(
        combus::wire::kCombusSystemComBusMd5,
        wireMd5,
        combus::wire::kProjectVersionMajor);
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

    // --- Group H: RX write identity by link layer (chantier 0.1) ---
    RUN_TEST(test_rx_system_link_applies_system_channels);
    RUN_TEST(test_rx_base_link_refuses_system_channels);
    RUN_TEST(test_rx_board_link_gets_system_identity);

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

    // --- Group I: links from their description (chantier 0.4) ---
    RUN_TEST(test_link_build_inter_node_tx);
    RUN_TEST(test_link_build_rx_only);
    RUN_TEST(test_link_build_intra_node_tx_rx);
    RUN_TEST(test_link_scope_layer);
    RUN_TEST(test_link_build_refuses_bad_wiring);

    // --- Group J: receiving link health (chantier 1.1) ---
    RUN_TEST(test_link_health_lost_until_first_frame);
    RUN_TEST(test_link_health_alive_after_frame);
    RUN_TEST(test_link_health_lost_after_silence);
    RUN_TEST(test_link_health_ignores_tx_only_link);

    // --- Group K: denied writes are counted (chantier 1.1) ---
    RUN_TEST(test_denied_write_is_counted);

    // --- Group L: both_or merge, anti ping-pong (chantier 1.1b) ---
    RUN_TEST(test_both_or_merges_peer_value);
    RUN_TEST(test_both_or_falls_back_when_peer_clears);
    RUN_TEST(test_both_or_ignores_silent_peer);
    RUN_TEST(test_both_or_sends_no_echo);
    RUN_TEST(test_both_or_board_never_writing_does_not_contribute);

    // --- Group M: link loss policy (chantier 1.1c) ---
    RUN_TEST(test_loss_action_by_scope);
    RUN_TEST(test_loss_row1_failsafe_link_in_grace);
    RUN_TEST(test_loss_row2_failsafe_link_never_seen_after_grace);
    RUN_TEST(test_loss_rows3to5_failsafe_link_alive_lost_back);
    RUN_TEST(test_loss_row6_idle_link_never_seen);
    RUN_TEST(test_loss_rows7to8_idle_link_lost_back);
    RUN_TEST(test_loss_row9_none_link_never_contributes);
    RUN_TEST(test_loss_row10_inter_link_failsafe_imposed);
    RUN_TEST(test_loss_row11_tx_only_link_never_contributes);

    // --- Group N: link contract, ACK and blocking (chantier 1.2) ---
    RUN_TEST(test_contract_row1_request_ack_stable);
    RUN_TEST(test_contract_row2_peer_late);
    RUN_TEST(test_contract_row3_md5_mismatch);
    RUN_TEST(test_contract_row4_minor_differs);
    RUN_TEST(test_contract_row5_major_differs);
    RUN_TEST(test_contract_row6_data_ignored_until_stable);
    RUN_TEST(test_contract_row7_silence_then_renegotiation);
    RUN_TEST(test_contract_row8_bad_handshake_while_stable);
    RUN_TEST(test_contract_row9_no_ack_means_lost);
    RUN_TEST(test_contract_row10_rx_only_end);
    RUN_TEST(test_contract_row11_tx_only_end);
    RUN_TEST(test_contract_row12_no_reply_to_ack);
    RUN_TEST(test_contract_failsafe_keeps_contract);

    // Chantier 12.6: Group F (FS1 failsafe re-arm) removed — see
    // header docstring above (5 independent test groups A/B/C/D/E).

    UNITY_END();
}

void loop() {}

// EOF test_combus_loopback.cpp