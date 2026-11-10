/******************************************************************************
 * @file test_remote_link.cpp
 * @brief Remote: its link to the machine it drives.
 *
 * @details The board is a remote (IS_REMOTE) driving a dumper truck: its
 *   ComBus is the BASE view of that type, mirrored (what the machine
 *   receives, the remote sends).  The link carries only what the remote sends,
 *   with its own table and MD5: each end announces its side
 *   and expects the other.  On the TX<->RX jumper (GPIO 17 <-> 16) the remote
 *   hears its own side and refuses it; made to announce the machine side, the
 *   handshake becomes STABLE and its frames make the link alive.  This does
 *   not prove the agreement with a real machine (pytest does, on the MD5).
 *
 *   Rows of the table validated by the owner: the link with the machine,
 *   plus the generated mirror seen from the C++ side.
 *   Run: pio test -e test_remote_link
 *****************************************************************************/

#include <Arduino.h>
#include <unity.h>

#include <core/system/combus/protocol/link/combus_link_watch.h>
#include <core/system/combus/protocol/handshake/combus_handshake.h>
#include <remotes/com/remote_links.h>
#include <remotes/config/driven_machines.h>

#include "combus.h"
#include "combus_peer_links.h"


static constexpr uint8_t kBrake = static_cast<uint8_t>(AnalogComBusID::BRAKE_BUS);
static constexpr uint8_t kKey   = static_cast<uint8_t>(DigitalComBusID::KEY_BTN);


/** Row 1: the remote sends the inputs of the machine: its BASE channels go out (uplink), none comes in. */
static void test_remote_row1_inputs_are_sent(void) {
    TEST_ASSERT_TRUE(comBus.analogBus[kBrake].flow.betweenNodes == Direction::Uplink);   // downlink_pt on the machine
    TEST_ASSERT_TRUE(comBus.digitalBus[kKey].flow.betweenNodes == Direction::Uplink);
}

/** Row 2: on the jumper, the remote hears its own side: the handshake refuses it. */
static void test_remote_row2_its_own_side_is_refused(void) {
    TEST_ASSERT_EQUAL_UINT8(1u, remote_links_init());
    ComLinkView v;
    for (uint32_t t0 = millis(); (millis() - t0) < 1000u; ) {
        remote_links_update();
        delay(5u);
    }
    TEST_ASSERT_TRUE(combus_links_view(0u, v));
    TEST_ASSERT_EQUAL_STRING("machine", v.name);
    TEST_ASSERT_NOT_NULL(v.peer);                                       // its own table, no pairing
    TEST_ASSERT_EQUAL_STRING("remote_dumper_truck", v.peer->coupler);
    TEST_ASSERT_TRUE(v.peer->role == ComLinkRole::PLUG);
    TEST_ASSERT_TRUE(remote_links_runtime(0u)->handshake.everReceived); // heard, and refused
    TEST_ASSERT_FALSE(v.contractStable);
}

/** Row 8: made to announce the machine side, the link to the machine: handshake STABLE, then alive. */
static void test_remote_row8_link_on_the_jumper(void) {
    remote_links_runtime(0u)->handshake.announced = combus::peer::kRemoteLink.expected;
    ComLinkView v;
    for (uint32_t t0 = millis(); (millis() - t0) < 2000u; ) {
        remote_links_update();
        delay(5u);
        if (combus_links_view(0u, v) && v.contractStable && v.state == ComLinkState::ALIVE) { break; }
    }
    TEST_ASSERT_TRUE(combus_links_view(0u, v));
    TEST_ASSERT_TRUE_MESSAGE(v.contractStable, "handshake not STABLE - check TX<->RX jumper");
    TEST_ASSERT_TRUE(v.state == ComLinkState::ALIVE);
}


void setup() {
    delay(2000u);
    UNITY_BEGIN();
    RUN_TEST(test_remote_row1_inputs_are_sent);
    RUN_TEST(test_remote_row2_its_own_side_is_refused);
    RUN_TEST(test_remote_row8_link_on_the_jumper);
    UNITY_END();
}

void loop() {}

// EOF test_remote_link.cpp
