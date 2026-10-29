/******************************************************************************
 * @file test_peer_link.cpp
 * @brief Link between two nodes through the generated tables (link rework 3.1b).
 *
 * @details The board is a fictitious TEST tractor; its peer type is the TEST
 *   trailer (core/config/machines/test_tractor/, test_trailer/).  The builder
 *   computed the channel set of that link and its table (combus_peer_links.h):
 *     analog  0: TEST_BRAKE_BUS (the tractor sends it), 1: TEST_TILT_ANGLE (it receives it);
 *     digital 0: TEST_COUPLED_FAIL (both_or), 1: TEST_LIGHTS (it sends it).
 *   TEST_THROTTLE_BUS and TEST_BEACON are in the tractor ComBus but not in the
 *   link; they come before channels of the link in the local order, so the
 *   table is not the identity for either type.
 *
 *   Rows of the table validated by the owner (2026-10-25):
 *     1  the generated descriptor;
 *     2  emission in wire order, through the table;
 *     3  reception through the table, only what the tractor receives;
 *     4  a channel outside the link is neither sent nor written;
 *     5  the contract of the link is the MD5 of the link;
 *     6  both_or through the table, no echo;
 *     7  end to end on the loopback (handshake with the link MD5, data).
 *   Row 8 (a link without peer is unchanged) is test_combus_loopback.
 *   Then the loss of the link (3.2c, table of 2026-10-26): the received
 *   positions take back their safe value.
 *   Then the accessors of the logical value (step S3-4, table of 2026-10-29):
 *   TEST_GEAR_BUS declares `zeroAt: neutral` (logical zero at CbusNeutral);
 *   combus_get_analog / combus_set_analog apply the offset, the raw accessors
 *   and the transport do not.
 *
 *   What the trailer would send is forged.  Requires the TX<->RX jumper on
 *   Serial2 (GPIO 17 <-> 16) for rows 6 and 7.  Run: pio test -e test_peer_link
 *****************************************************************************/

#include <Arduino.h>
#include <unity.h>
#include <string.h>

#include <core/system/combus/protocol/frame/combus_frame.h>
#include <core/system/combus/protocol/frame/combus_handshake.h>
#include <core/system/combus/protocol/frame/combus_handshake_tx.h>
#include <core/system/hw/transport/uart_com.h>
#include <core/system/combus/protocol/combus_tx.h>
#include <core/system/combus/protocol/combus_rx.h>
#include <core/system/combus/protocol/combus_links.h>
#include <core/system/combus/combus_access.h>
#include <core/system/combus/combus_res.h>     // CbusNeutral (safe value of an analog channel, 3.2c)
#include <core/system/com/com_manager.h>

#include "combus.h"
#include "combus_peer_links.h"

using combus::peer::TestTrailer::kLink;


// =============================================================================
// FIXTURES
// =============================================================================

static constexpr int      kTxPin    = 17;
static constexpr int      kRxPin    = 16;
static constexpr uint32_t kRxPollMs = 50u;

static constexpr uint8_t kBrake    = static_cast<uint8_t>(AnalogComBusID::TEST_BRAKE_BUS);
static constexpr uint8_t kTilt     = static_cast<uint8_t>(AnalogComBusID::TEST_TILT_ANGLE);
static constexpr uint8_t kThrottle = static_cast<uint8_t>(AnalogComBusID::TEST_THROTTLE_BUS);
static constexpr uint8_t kFault    = static_cast<uint8_t>(DigitalComBusID::TEST_COUPLED_FAIL);
static constexpr uint8_t kLights   = static_cast<uint8_t>(DigitalComBusID::TEST_LIGHTS);
static constexpr uint8_t kBeacon   = static_cast<uint8_t>(DigitalComBusID::TEST_BEACON);

static constexpr ComBusFrameCfg kLinkCfg = { 2u, 2u };

static UartCtx        s_uartPool[1];
static CombusTxState  s_txStates[1];
static CombusRxState  s_rxStates[1];
static NodeCom*       s_com = nullptr;
static constexpr uint8_t kLinkIdx = 0u;

static ComBus& bus = comBus;

static constexpr ComProtocolCfg kProto = { 115200u, 50u, 200u, 200u };
static constexpr ComDev kTrailerDev = {
    "test_trailer", LinkScope::INTER_NODE, LinkType::BIDIRECTIONAL,
    &kProto, LinkActivation::ALWAYS, nullptr, &kLink
};
static const ComPort s_port = ComPort::makeUart("test", { 2, kTxPin, kRxPin, 115200u });
static ComLinkRuntime s_runtime;
static ComBusLink     s_link;

static uint16_t frameAnalog[2]  = {};
static bool     frameDigital[2] = {};
static ComBusFrame s_frame = { .analog = frameAnalog, .digital = frameDigital };
static uint8_t  s_buf[CombusRxBufSize];

/** Reset the tractor ComBus values (the layout of the generated arrays is kept). */
static void resetBus(void) {
    for (uint8_t i = 0u; i < static_cast<uint8_t>(AnalogComBusID::CH_COUNT); ++i) { bus.analogBus[i].value = 0u; }
    for (uint8_t i = 0u; i < static_cast<uint8_t>(DigitalComBusID::CH_COUNT); ++i) {
        bus.digitalBus[i].value = false;
        bus.digitalBus[i].bothOr.local = bus.digitalBus[i].bothOr.written = bus.digitalBus[i].bothOr.remote = false;
    }
}

/** A frame as the trailer would send it: wire order of the link. */
static uint8_t encodeTrailerFrame(uint16_t brake, uint16_t tilt, bool fault, bool lights, uint8_t seq) {
    static AnalogComBus  a[2];
    static DigitalComBus d[2];
    static ComBus trailer = { .analogBus = a, .digitalBus = d };
    a[0].value = brake;  a[1].value = tilt;
    d[0].value = fault;  d[1].value = lights;
    trailer.analogBusMaxVal = bus.analogBusMaxVal;
    return combus_frame_encode(kLinkCfg, s_buf, &trailer, seq, 2u, 2u);
}

/** Build the link with the trailer on the loopback port and put it under watch. */
static void startLink(void) {
    resetBus();
    const ComDevMap map = { &kTrailerDev, &s_port, LinkEnd::TX_RX };
    TEST_ASSERT_TRUE(combus_link_build(map, s_com, s_runtime, s_link));
    s_link.target = &bus;
    combus_protocol_init(kLinkIdx, &s_link);
    combus_links_watch(&s_runtime, 1u);
}

/** Run the handshake on the loopback until the contract is STABLE. */
static void ensureStable(void) {
    CombusHandshakeContext* ctx = &s_runtime.handshake;
    for (uint32_t t0 = millis(); !combus_handshake_is_contract_validated(ctx) && (millis() - t0) < 1500u; ) {
        combus_handshake_tx_update(ctx, s_com);
        delay(10u);
        combus_rx_update(&bus);
    }
    TEST_ASSERT_TRUE_MESSAGE(combus_handshake_is_contract_validated(ctx), "link not established - check TX<->RX jumper");
}


// =============================================================================
// TESTS
// =============================================================================

/** Row 1: the descriptor generated for the link with the trailer. */
static void test_row1_generated_descriptor(void) {
    TEST_ASSERT_EQUAL_UINT8(2u, kLink.nAnalog);
    TEST_ASSERT_EQUAL_UINT8(2u, kLink.nDigital);
    TEST_ASSERT_EQUAL_UINT8(kBrake,  kLink.analog[0]);
    TEST_ASSERT_EQUAL_UINT8(kTilt,   kLink.analog[1]);
    TEST_ASSERT_EQUAL_UINT8(kFault,  kLink.digital[0]);
    TEST_ASSERT_EQUAL_UINT8(kLights, kLink.digital[1]);
    TEST_ASSERT_FALSE(kLink.analogRx[0]);       // the tractor sends the brake
    TEST_ASSERT_TRUE(kLink.analogRx[1]);        // it receives the tilt
    TEST_ASSERT_TRUE(kLink.digitalRx[0]);       // the shared fault (OR)
    TEST_ASSERT_FALSE(kLink.digitalRx[1]);      // it sends the lights

        // The test only bites if the wire order differs from the local order.
    TEST_ASSERT_TRUE_MESSAGE(kTilt != 1u && kFault != 0u, "the table is the identity: the tests cannot bite");
}

/** Rows 2 and 4: emission in wire order, through the table; the throttle is not in the frame. */
static void test_row2_emission_through_the_table(void) {
    resetBus();
    bus.analogBus[kBrake].value    = 111u;
    bus.analogBus[kTilt].value     = 222u;
    bus.analogBus[kThrottle].value = 999u;
    bus.digitalBus[kLights].value  = true;

    const uint8_t len = combus_frame_encode(kLinkCfg, s_buf, &bus, 3u, 2u, 2u, nullptr, kLink.analog, kLink.digital);
    TEST_ASSERT_TRUE(combus_frame_decode(kLinkCfg, &s_frame, s_buf, len));
    TEST_ASSERT_EQUAL_UINT8(2u, s_frame.header.cfg.nAnalog);
    TEST_ASSERT_EQUAL_UINT16(111u, frameAnalog[0]);
    TEST_ASSERT_EQUAL_UINT16(222u, frameAnalog[1]);
    TEST_ASSERT_FALSE(frameDigital[0]);
    TEST_ASSERT_TRUE(frameDigital[1]);
}

/** Rows 3 and 4: reception through the table, only what the tractor receives. */
static void test_row3_reception_through_the_table(void) {
    resetBus();
    bus.analogBus[kBrake].value    = 111u;          // its own brake
    bus.analogBus[kThrottle].value = 999u;
    bus.digitalBus[kBeacon].value  = false;

    const uint8_t len = encodeTrailerFrame(500u, 333u, false, true, 4u);
    TEST_ASSERT_TRUE(combus_frame_decode(kLinkCfg, &s_frame, s_buf, len));
    combus_frame_apply(kLinkCfg, &bus, &s_frame, ChanLayer::BASE, &kLink);

    TEST_ASSERT_EQUAL_UINT16(333u, bus.analogBus[kTilt].value);       // received
    TEST_ASSERT_EQUAL_UINT16(111u, bus.analogBus[kBrake].value);      // sent by the tractor: kept
    TEST_ASSERT_FALSE(bus.digitalBus[kLights].value);                 // sent by the tractor: kept
    TEST_ASSERT_EQUAL_UINT16(999u, bus.analogBus[kThrottle].value);   // not in the link
    TEST_ASSERT_FALSE(bus.digitalBus[kBeacon].value);                 // not in the link
}

/** Row 5: the contract of the link is the MD5 of the link. */
static void test_row5_contract_is_the_link_md5(void) {
    startLink();
    const CombusHandshakeContext& ctx = s_runtime.handshake;
    TEST_ASSERT_TRUE(ctx.peer == &kLink);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kLink.md5Stable, ctx.expectedMd5, 16u);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kLink.md5, ctx.expectedMd5Next, 16u);
    TEST_ASSERT_FALSE(ctx.hasCandidates);
    TEST_ASSERT_TRUE(s_link.rxCfg.has_value());
    TEST_ASSERT_EQUAL_UINT8(2u, s_link.rxCfg->nAnalog);
    TEST_ASSERT_EQUAL_UINT8(2u, s_link.rxCfg->nDigital);
}

/** Row 6: the fault raised by the trailer is merged through the table; the tractor does not echo it. */
static void test_row6_both_or_through_the_table(void) {
    startLink();
    ensureStable();

    const uint8_t len = encodeTrailerFrame(0u, 0u, true, false, 5u);
    s_com->write(s_com->ctx, s_buf, len);
    delay(kRxPollMs);
    com_rx_update(bus);
    TEST_ASSERT_TRUE(bus.digitalBus[kFault].value);
    TEST_ASSERT_FALSE(bus.digitalBus[kFault].bothOr.local);

    delay(30u);
    combus_tx_update(&bus);                                         // loops back on the same link
    delay(kRxPollMs);
    combus_rx_update(&bus);
    const ComBusFrame* snap = combus_rx_snapshot(kLinkIdx);
    TEST_ASSERT_NOT_NULL(snap);
    TEST_ASSERT_FALSE_MESSAGE(snap->digital[0], "the trailer's fault was echoed back to it");
}

/** Row 7: end to end on the loopback: handshake with the link MD5, then data in wire order. */
static void test_row7_end_to_end(void) {
    startLink();
    ensureStable();

    bus.analogBus[kBrake].value = 555u;
    bus.analogBus[kTilt].value  = 444u;
    delay(30u);
    combus_tx_update(&bus);
    delay(kRxPollMs);
    combus_rx_update(&bus);

    const ComBusFrame* snap = combus_rx_snapshot(kLinkIdx);
    TEST_ASSERT_NOT_NULL(snap);
    TEST_ASSERT_EQUAL_UINT8(2u, snap->header.cfg.nAnalog);
    TEST_ASSERT_EQUAL_UINT16(555u, snap->analog[0]);
    TEST_ASSERT_EQUAL_UINT16(444u, snap->analog[1]);
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::ALIVE);
}


// =============================================================================
// LOSS OF THE LINK (link rework 3.2c)
//
// Table validated by the owner (2026-10-26): at the loss of a link with a peer
// (silence, or contract dropped), the positions this node receives and writes
// in its ComBus take back their safe value (CbusNeutral / false); the
// positions it sends do not move; both_or is unchanged (a lost link
// contributes nothing to the merge).
// =============================================================================

/** The trailer sends one frame (tilt, fault) and com_rx_update() receives it. */
static void receiveFromTrailer(uint16_t tilt, bool fault, uint8_t seq) {
    const uint8_t len = encodeTrailerFrame(0u, tilt, fault, false, seq);
    s_com->write(s_com->ctx, s_buf, len);
    delay(kRxPollMs);
    com_rx_update(bus);
}

/** Rows 1 to 3: the trailer goes silent: the tilt it sent takes back its safe value, nothing else moves. */
static void test_loss_row1_to_3_received_positions_take_back_their_safe_value(void) {
    startLink();
    ensureStable();
    bus.analogBus[kBrake].value    = 111u;
    bus.analogBus[kThrottle].value = 999u;
    bus.digitalBus[kLights].value  = true;
    bus.digitalBus[kBeacon].value  = true;

    receiveFromTrailer(333u, false, 6u);
    TEST_ASSERT_EQUAL_UINT16(333u, bus.analogBus[kTilt].value);

    delay(kProto.linkTimeoutMs + 60u);
    com_rx_update(bus);
    TEST_ASSERT_TRUE(combus_link_state(kLinkIdx) == ComLinkState::LOST);

    TEST_ASSERT_EQUAL_UINT16(CbusNeutral, bus.analogBus[kTilt].value);   // received: safe value
    TEST_ASSERT_EQUAL_UINT16(111u, bus.analogBus[kBrake].value);         // sent by the tractor: kept
    TEST_ASSERT_TRUE(bus.digitalBus[kLights].value);                     // sent by the tractor: kept
    TEST_ASSERT_EQUAL_UINT16(999u, bus.analogBus[kThrottle].value);      // not in the link
    TEST_ASSERT_TRUE(bus.digitalBus[kBeacon].value);                     // not in the link
}

/** Row 4: the fault raised by the trailer falls back when the link is lost (merge unchanged). */
static void test_loss_row4_shared_fault_falls_back(void) {
    startLink();
    ensureStable();
    receiveFromTrailer(0u, true, 7u);
    TEST_ASSERT_TRUE(bus.digitalBus[kFault].value);

    delay(kProto.linkTimeoutMs + 60u);
    com_rx_update(bus);
    TEST_ASSERT_FALSE(bus.digitalBus[kFault].value);
    TEST_ASSERT_FALSE(bus.digitalBus[kFault].bothOr.local);
}

/** Row 5: the trailer comes back: its frames are applied again. */
static void test_loss_row5_reconnection(void) {
    startLink();
    ensureStable();
    receiveFromTrailer(333u, false, 8u);
    delay(kProto.linkTimeoutMs + 60u);
    com_rx_update(bus);
    TEST_ASSERT_EQUAL_UINT16(CbusNeutral, bus.analogBus[kTilt].value);

    ensureStable();
    receiveFromTrailer(444u, false, 9u);
    TEST_ASSERT_EQUAL_UINT16(444u, bus.analogBus[kTilt].value);
}

/** Row 6: the contract drops (no ACK) before any silence: same safe value. */
static void test_loss_row6_contract_dropped(void) {
    startLink();
    ensureStable();
    receiveFromTrailer(333u, false, 10u);
    TEST_ASSERT_EQUAL_UINT16(333u, bus.analogBus[kTilt].value);

    combus_handshake_internal::dropContract(&s_runtime.handshake, "test: no ACK");
    com_rx_update(bus);
    TEST_ASSERT_FALSE(combus_link_state(kLinkIdx) == ComLinkState::ALIVE);
    TEST_ASSERT_EQUAL_UINT16(CbusNeutral, bus.analogBus[kTilt].value);
}


// =============================================================================
// ACCESSORS OF THE LOGICAL VALUE (step S3-4)
// =============================================================================

static constexpr AnalogComBusID kGearId  = AnalogComBusID::TEST_GEAR_BUS;
static constexpr AnalogComBusID kBrakeId = AnalogComBusID::TEST_BRAKE_BUS;

static uint16_t rawOf(AnalogComBusID id) { return bus.analogBus[static_cast<uint8_t>(id)].value; }
static void     setRaw(AnalogComBusID id, uint16_t v) { bus.analogBus[static_cast<uint8_t>(id)].value = v; }


void test_zero_row1_flag_generated(void) {
    TEST_ASSERT_TRUE(bus.analogBus[static_cast<uint8_t>(kGearId)].zeroAtNeutral);
    TEST_ASSERT_FALSE(bus.analogBus[static_cast<uint8_t>(kBrakeId)].zeroAtNeutral);
}


void test_zero_row2_read_logical(void) {
    setRaw(kGearId, CbusNeutral);
    TEST_ASSERT_EQUAL_INT32(0, combus_get_analog(bus, kGearId));
    setRaw(kGearId, CbusNeutral + 2u);
    TEST_ASSERT_EQUAL_INT32(2, combus_get_analog(bus, kGearId));
    setRaw(kGearId, CbusNeutral - 767u);
    TEST_ASSERT_EQUAL_INT32(-767, combus_get_analog(bus, kGearId));      // negative: a real value
    setRaw(kGearId, 0u);
    TEST_ASSERT_EQUAL_INT32(-static_cast<int32_t>(CbusNeutral), combus_get_analog(bus, kGearId));
    setRaw(kGearId, CbusMaxVal);
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(CbusMaxVal) - static_cast<int32_t>(CbusNeutral),
                            combus_get_analog(bus, kGearId));
}


void test_zero_row3_write_logical(void) {
    TEST_ASSERT_TRUE(combus_set_analog(bus, kGearId, 3, ChanLayer::BASE));
    TEST_ASSERT_EQUAL_UINT16(CbusNeutral + 3u, rawOf(kGearId));
    TEST_ASSERT_TRUE(combus_set_analog(bus, kGearId, -1, ChanLayer::BASE));
    TEST_ASSERT_EQUAL_UINT16(CbusNeutral - 1u, rawOf(kGearId));
}


void test_zero_row4_write_is_bounded(void) {
    combus_set_analog(bus, kGearId, 70000, ChanLayer::BASE);
    TEST_ASSERT_EQUAL_UINT16(CbusMaxVal, rawOf(kGearId));
    combus_set_analog(bus, kGearId, -40000, ChanLayer::BASE);
    TEST_ASSERT_EQUAL_UINT16(0u, rawOf(kGearId));
}


void test_zero_row5_channel_without_zero_at_unchanged(void) {
    combus_set_analog(bus, kBrakeId, 111, ChanLayer::BASE);
    TEST_ASSERT_EQUAL_UINT16(111u, rawOf(kBrakeId));
    TEST_ASSERT_EQUAL_INT32(111, combus_get_analog(bus, kBrakeId));
    combus_set_analog(bus, kBrakeId, -5, ChanLayer::BASE);                // no negative without zeroAt: bounded at 0
    TEST_ASSERT_EQUAL_UINT16(0u, rawOf(kBrakeId));
}


void test_zero_row6_raw_accessors_never_offset(void) {
    TEST_ASSERT_TRUE(combus_set_raw_analog(bus, kGearId, 1234u, ChanLayer::BASE));
    TEST_ASSERT_EQUAL_UINT16(1234u, rawOf(kGearId));
    TEST_ASSERT_EQUAL_UINT16(1234u, combus_get_raw_analog(bus, kGearId));
}


void test_zero_row7_layer_check_kept(void) {
    setRaw(AnalogComBusID::RUNLEVEL, 0u);
    TEST_ASSERT_FALSE(combus_set_analog(bus, AnalogComBusID::RUNLEVEL, 5, ChanLayer::BASE));   // BASE may not write SYSTEM
    TEST_ASSERT_EQUAL_UINT16(0u, rawOf(AnalogComBusID::RUNLEVEL));
}


// =============================================================================
// RUNNER
// =============================================================================

void setup() {
    delay(2000u);
    UNITY_BEGIN();

    uart_com_register_pool(s_uartPool, 1);
    s_com = uart_com_init(&Serial2, 115200u, kTxPin, kRxPin, "test_peer_link");
    combus_tx_register_pool(s_txStates, 1);
    combus_rx_register_pool(s_rxStates, 1);

    RUN_TEST(test_row1_generated_descriptor);
    RUN_TEST(test_row2_emission_through_the_table);
    RUN_TEST(test_row3_reception_through_the_table);
    RUN_TEST(test_row5_contract_is_the_link_md5);
    RUN_TEST(test_row6_both_or_through_the_table);
    RUN_TEST(test_row7_end_to_end);
    RUN_TEST(test_loss_row1_to_3_received_positions_take_back_their_safe_value);
    RUN_TEST(test_loss_row4_shared_fault_falls_back);
    RUN_TEST(test_loss_row5_reconnection);
    RUN_TEST(test_loss_row6_contract_dropped);

    RUN_TEST(test_zero_row1_flag_generated);
    RUN_TEST(test_zero_row2_read_logical);
    RUN_TEST(test_zero_row3_write_logical);
    RUN_TEST(test_zero_row4_write_is_bounded);
    RUN_TEST(test_zero_row5_channel_without_zero_at_unchanged);
    RUN_TEST(test_zero_row6_raw_accessors_never_offset);
    RUN_TEST(test_zero_row7_layer_check_kept);

    UNITY_END();
}

void loop() {}

// EOF test_peer_link.cpp
