/******************************************************************************
 * @file test_remote_link.cpp
 * @brief Remote embryo: its link to the machine it drives (chantier 4).
 *
 * @details The board is a remote (IS_REMOTE) driving a dumper truck: its
 *   ComBus is the BASE view of that type, mirrored (what the machine
 *   receives, the remote sends), and its BASE MD5 is the one of the machine.
 *   On the TX<->RX jumper (GPIO 17 <-> 16) the remote hears itself: the
 *   handshake of the plain BASE link becomes STABLE (the same MD5 both ways)
 *   and its frames make the link alive.
 *
 *   Rows of the table validated by the owner (2026-11-01): R8, plus the
 *   generated mirror seen from the C++ side.  Run: pio test -e test_remote_link
 *****************************************************************************/

#include <Arduino.h>
#include <unity.h>

#include <core/system/combus/protocol/combus_links.h>
#include <core/system/combus/protocol/frame/combus_handshake.h>
#include <remotes/com/remote_links.h>
#include <remotes/config/driven_machines.h>

#include "combus.h"
#include "combus_base_md5.h"


static constexpr uint8_t kBrake = static_cast<uint8_t>(AnalogComBusID::BRAKE_BUS);
static constexpr uint8_t kKey   = static_cast<uint8_t>(DigitalComBusID::KEY_BTN);


/** Row 1: the remote sends the inputs of the machine: its BASE channels go out (uplink), none comes in. */
static void test_remote_row1_inputs_are_sent(void) {
    TEST_ASSERT_TRUE(comBus.analogBus[kBrake].flow.betweenNodes == Direction::Uplink);   // downlink_pt on the machine
    TEST_ASSERT_TRUE(comBus.digitalBus[kKey].flow.betweenNodes == Direction::Uplink);
}

/** Row 8: on the jumper, the plain BASE link to the machine: handshake STABLE, then alive. */
static void test_remote_row8_link_on_the_jumper(void) {
    TEST_ASSERT_EQUAL_UINT8(1u, remote_links_init());
    ComLinkView v;
    for (uint32_t t0 = millis(); (millis() - t0) < 2000u; ) {
        remote_links_update();
        delay(5u);
        if (combus_links_view(0u, v) && v.contractStable && v.state == ComLinkState::ALIVE) { break; }
    }
    TEST_ASSERT_TRUE(combus_links_view(0u, v));
    TEST_ASSERT_EQUAL_STRING("machine", v.name);
    TEST_ASSERT_NULL(v.peer);                                           // a plain BASE link: no coupler
    TEST_ASSERT_TRUE_MESSAGE(v.contractStable, "handshake not STABLE - check TX<->RX jumper");
    TEST_ASSERT_TRUE(v.state == ComLinkState::ALIVE);
}


void setup() {
    delay(2000u);
    UNITY_BEGIN();
    RUN_TEST(test_remote_row1_inputs_are_sent);
    RUN_TEST(test_remote_row8_link_on_the_jumper);
    UNITY_END();
}

void loop() {}

// EOF test_remote_link.cpp
