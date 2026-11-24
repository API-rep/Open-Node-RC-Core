/******************************************************************************
 * @file  test_remote_source.cpp
 * @brief Base of the command sources: one source alive drives the inputs of the type.
 *
 * @details Two scripted fake sources (A and B) stand for the modules: the test says
 *   when each one is alive and what it gives.  Their mapper writes THROTTLE_STICK and
 *   HORN_BTN, BOARD inputs of the dumper truck, with the identity of the board.  One
 *   call of remote_sources_update() = one cycle.
 *
 *   Rows (doc/WIP - Local remote.md §9, R2):
 *     1  one source alive: its values reach the inputs, NODE_LINK_FAIL false;
 *     2  the source alive drives, whatever its rank;
 *     3  loss: the inputs back to their safe value, NODE_LINK_FAIL true;
 *     4  two sources alive: no mapper runs, safe values, NODE_LINK_FAIL true;
 *     5  return: the source drives again, NODE_LINK_FAIL false;
 *     6  no source, or too many: NODE_LINK_FAIL true;
 *     7  a link to another node lost: NODE_LINK_FAIL true, the source still drives.
 *
 *   No jumper needed.  Run: pio test -e test_remote_source
 *****************************************************************************/

#include <Arduino.h>
#include <unity.h>
#include <iterator>   // std::size

#include "combus.h"
#include <core/system/combus/combus_access.h>
#include <core/system/combus/combus_res.h>                            // CbusNeutral
#include <core/system/combus/protocol/link/combus_link_supervisor.h>  // combus_link_supervisor_start()
#include <core/system/input/remote_source.h>



// =============================================================================
// FIXTURE
// =============================================================================

	/// A scripted source: alive or not, and what it gives.
struct FakeSource {
	bool     alive    = false;
	uint16_t throttle = 0u;
	bool     horn     = false;
	uint16_t polls    = 0u;
	uint16_t applies  = 0u;
};


static bool fakePoll( ComBus& /*bus*/, void* ctx )
{
	FakeSource* f = static_cast<FakeSource*>(ctx);
	++f->polls;

	return f->alive;
}


static void fakeApply( ComBus& bus, void* ctx )
{
	FakeSource* f = static_cast<FakeSource*>(ctx);
	++f->applies;

	TEST_ASSERT_TRUE_MESSAGE(combus_set_raw_analog(bus, AnalogComBusID::THROTTLE_STICK, f->throttle),
	                         "the mapper of a source cannot write a BOARD input");
	TEST_ASSERT_TRUE(combus_set_digital(bus, DigitalComBusID::HORN_BTN, f->horn));
}


static FakeSource s_a;
static FakeSource s_b;

static const RemoteSourceDev kDevs[] = {
	{ "fake_a", fakePoll, fakeApply, &s_a },
	{ "fake_b", fakePoll, fakeApply, &s_b },
};

static const AnalogComBusID  kAnalogInputs[]  = { AnalogComBusID::THROTTLE_STICK, AnalogComBusID::STEERING_STICK };
static const DigitalComBusID kDigitalInputs[] = { DigitalComBusID::HORN_BTN, DigitalComBusID::KEY_BTN };

static RemoteSources s_sources;


static uint16_t analog( AnalogComBusID id )   { return combus_get_raw_analog(comBus, id); }
static bool     digital( DigitalComBusID id ) { return comBus.digitalBus[static_cast<uint8_t>(id)].value; }
static bool     nodeLinkFail( void )          { return digital(DigitalComBusID::NODE_LINK_FAIL); }

	/** One cycle of the base. */
static void cycle( void ) { remote_sources_update(s_sources, comBus); }


	/** Inputs away from their safe value, so that a safe value is seen being written. */
static void moveInputs( void )
{
	combus_set_raw_analog(comBus, AnalogComBusID::THROTTLE_STICK, 1000u);
	combus_set_raw_analog(comBus, AnalogComBusID::STEERING_STICK, 2000u);
	combus_set_digital(comBus, DigitalComBusID::HORN_BTN, true);
	combus_set_digital(comBus, DigitalComBusID::KEY_BTN, true);
}


	/** Inputs at their safe value: sticks at neutral, buttons released. */
static void assertSafeValues( void )
{
	TEST_ASSERT_EQUAL_UINT16(CbusNeutral, analog(AnalogComBusID::THROTTLE_STICK));
	TEST_ASSERT_EQUAL_UINT16(CbusNeutral, analog(AnalogComBusID::STEERING_STICK));
	TEST_ASSERT_FALSE(digital(DigitalComBusID::HORN_BTN));
	TEST_ASSERT_FALSE(digital(DigitalComBusID::KEY_BTN));
}


	/** Fresh start: both sources dead, the contributor at its default (fault), no link watched. */
void setUp( void )
{
	s_a = {};
	s_b = {};
	s_a.throttle = 50000u;
	s_a.horn     = true;
	s_b.throttle = 12000u;
	s_b.horn     = false;

	s_sources        = {};
	s_sources.devs   = kDevs;
	s_sources.count  = static_cast<uint8_t>(std::size(kDevs));
	s_sources.inputs = {
		{ kAnalogInputs,  static_cast<uint8_t>(std::size(kAnalogInputs)) },
		{ kDigitalInputs, static_cast<uint8_t>(std::size(kDigitalInputs)) },
	};

	combus_link_supervisor_start(nullptr, 0u);
	combus_set_digital(comBus, DigitalComBusID::NODE_LINK_FAIL, true);
	moveInputs();
}

void tearDown( void ) {}



// =============================================================================
// ROWS
// =============================================================================

void test_row1_one_source_alive_drives_the_inputs( void )
{
	s_a.alive = true;
	cycle();

	TEST_ASSERT_EQUAL(static_cast<int>(RemoteSourceState::ONE), static_cast<int>(s_sources.watch.state));
	TEST_ASSERT_EQUAL_UINT16(50000u, analog(AnalogComBusID::THROTTLE_STICK));
	TEST_ASSERT_TRUE(digital(DigitalComBusID::HORN_BTN));
	TEST_ASSERT_FALSE_MESSAGE(nodeLinkFail(), "one source drives: NODE_LINK_FAIL must be written false");
	TEST_ASSERT_EQUAL_UINT16(1u, s_a.applies);
}


void test_row2_the_source_alive_drives_whatever_its_rank( void )
{
	s_b.alive = true;
	cycle();

	TEST_ASSERT_EQUAL_UINT8(1u, s_sources.watch.active);
	TEST_ASSERT_EQUAL_UINT16(12000u, analog(AnalogComBusID::THROTTLE_STICK));
	TEST_ASSERT_FALSE(digital(DigitalComBusID::HORN_BTN));
	TEST_ASSERT_EQUAL_UINT16(0u, s_a.applies);
	TEST_ASSERT_EQUAL_UINT16(1u, s_b.applies);
	TEST_ASSERT_FALSE(nodeLinkFail());
}


void test_row3_loss_sends_the_inputs_back_to_their_safe_value( void )
{
	s_a.alive = true;
	cycle();

	s_a.alive = false;
	moveInputs();
	cycle();

	TEST_ASSERT_EQUAL(static_cast<int>(RemoteSourceState::NONE), static_cast<int>(s_sources.watch.state));
	assertSafeValues();
	TEST_ASSERT_TRUE(nodeLinkFail());
	TEST_ASSERT_EQUAL_UINT16(1u, s_a.applies);   // no mapper after the loss
}


void test_row4_two_sources_alive_block_the_driving( void )
{
	s_a.alive = true;
	s_b.alive = true;
	cycle();

	TEST_ASSERT_EQUAL(static_cast<int>(RemoteSourceState::SEVERAL), static_cast<int>(s_sources.watch.state));
	assertSafeValues();
	TEST_ASSERT_TRUE(nodeLinkFail());
	TEST_ASSERT_EQUAL_UINT16(0u, s_a.applies);
	TEST_ASSERT_EQUAL_UINT16(0u, s_b.applies);
	TEST_ASSERT_EQUAL_UINT16(1u, s_a.polls);    // both are asked
	TEST_ASSERT_EQUAL_UINT16(1u, s_b.polls);
}


void test_row5_return_the_source_drives_again( void )
{
	s_a.alive = true;
	cycle();
	s_a.alive = false;
	cycle();
	TEST_ASSERT_TRUE(nodeLinkFail());

	s_a.alive = true;
	cycle();

	TEST_ASSERT_EQUAL(static_cast<int>(RemoteSourceState::ONE), static_cast<int>(s_sources.watch.state));
	TEST_ASSERT_EQUAL_UINT16(50000u, analog(AnalogComBusID::THROTTLE_STICK));
	TEST_ASSERT_FALSE(nodeLinkFail());
}


void test_row5b_one_of_two_leaves_the_other_drives( void )
{
	s_a.alive = true;
	s_b.alive = true;
	cycle();
	TEST_ASSERT_TRUE(nodeLinkFail());

	s_a.alive = false;
	cycle();

	TEST_ASSERT_EQUAL_UINT8(1u, s_sources.watch.active);
	TEST_ASSERT_EQUAL_UINT16(12000u, analog(AnalogComBusID::THROTTLE_STICK));
	TEST_ASSERT_FALSE(nodeLinkFail());
}


void test_row6_no_source_declared_is_a_loss( void )
{
	s_sources.devs  = nullptr;
	s_sources.count = 0u;
	combus_set_digital(comBus, DigitalComBusID::NODE_LINK_FAIL, false);
	cycle();

	assertSafeValues();
	TEST_ASSERT_TRUE(nodeLinkFail());
}


void test_row6b_too_many_sources_is_a_loss( void )
{
	s_a.alive       = true;
	s_sources.count = kRemoteSourceMaxCount + 1u;   // devs is too short, but nothing is read
	combus_set_digital(comBus, DigitalComBusID::NODE_LINK_FAIL, false);
	cycle();

	TEST_ASSERT_EQUAL_UINT16(0u, s_a.polls);
	assertSafeValues();
	TEST_ASSERT_TRUE(nodeLinkFail());
}


void test_row7_a_lost_link_to_another_node_counts( void )
{
		// One receiving link to another node, never seen: lost at once
	static const ComDev   dev = { "node_link", LinkScope::INTER_NODE, LinkType::BIDIRECTIONAL, nullptr,
	                              LinkActivation::ALWAYS, nullptr };
	static ComLinkRuntime rt[1];

	rt[0]                     = {};
	rt[0].dev                 = &dev;
	rt[0].watch.receives      = true;
	rt[0].watch.linkTimeoutMs = 100u;
	combus_link_supervisor_start(rt, 1u);

	s_a.alive = true;
	cycle();

	TEST_ASSERT_TRUE_MESSAGE(nodeLinkFail(), "a lost link to another node must raise NODE_LINK_FAIL");
	TEST_ASSERT_EQUAL_UINT16(50000u, analog(AnalogComBusID::THROTTLE_STICK));   // the source still drives

	combus_link_supervisor_start(nullptr, 0u);
	cycle();
	TEST_ASSERT_FALSE(nodeLinkFail());
}



// =============================================================================
// RUNNER
// =============================================================================

void setup()
{
	delay(2000u);
	UNITY_BEGIN();

	RUN_TEST(test_row1_one_source_alive_drives_the_inputs);
	RUN_TEST(test_row2_the_source_alive_drives_whatever_its_rank);
	RUN_TEST(test_row3_loss_sends_the_inputs_back_to_their_safe_value);
	RUN_TEST(test_row4_two_sources_alive_block_the_driving);
	RUN_TEST(test_row5_return_the_source_drives_again);
	RUN_TEST(test_row5b_one_of_two_leaves_the_other_drives);
	RUN_TEST(test_row6_no_source_declared_is_a_loss);
	RUN_TEST(test_row6b_too_many_sources_is_a_loss);
	RUN_TEST(test_row7_a_lost_link_to_another_node_counts);

	UNITY_END();
}

void loop() {}

// EOF test_remote_source.cpp
