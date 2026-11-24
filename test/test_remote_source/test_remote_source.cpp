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
 *   Then a module (remote_module.h), with a fake backend that writes PS4_LY and
 *   PS4_CIRCLE, a reframer that adds an offset and counts its passes, and the
 *   tables (R3):
 *     m1  the module drives through the base: backend, reframer, then mapper;
 *     m2  the table of the instance replaces the one of the type;
 *     m3  a refused table at boot: the source is refused, never alive;
 *     m4  a refused table at the door: the previous one stays;
 *     m5  the checks: an input twice, a channel read twice, not an input, too long;
 *     m6  the reframer runs after a read that succeeded only;
 *     m7  one read per period;
 *     m8  the table of the type for the PS4 passes the checks;
 *     m9  the scaling of the PS4 axes;
 *     m10 the cost of a read with a reframer of six chains (measure, printed).
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
#include <core/system/input/remote_module.h>
#include <core/system/input/ps4/ps4_backend.h>                           // ps4_scale_axis()
#include <core/config/machines/dumper_truck/inputs/ps4_map.h>           // kPs4TypeMap, ps4_instance_map()
#include <core/system/combus/processors/math/cb_scale.h>                 // cb_scale_fn, for the cost
#include <core/system/combus/processors/proc_chain.h>                    // proc_chain_update()



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
// MODULE FIXTURE
// =============================================================================

	/// A scripted backend: connected or not, and what the controller gives.
struct FakeBackend {
	bool     connected = false;
	uint16_t ly        = CbusNeutral;
	bool     circle    = false;
	uint16_t reads     = 0u;
};

static FakeBackend s_backend;


	/// Rewrites both channels on every call, as the contract of refresh() wants.
static bool fakeRefresh( ComBus& bus, void* ctx )
{
	FakeBackend* b = static_cast<FakeBackend*>(ctx);
	++b->reads;

	combus_set_raw_analog(bus, AnalogComBusID::PS4_LY, b->connected ? b->ly : CbusNeutral);
	combus_set_digital(bus, DigitalComBusID::PS4_CIRCLE, b->connected && b->circle);

	return b->connected;
}


	/// The reframer: adds kOffset to the value, counts its passes in its state.
static constexpr uint16_t kOffset = 1000u;

static void offsetFn( CbProc* proc, uint16_t& value, bool& /*claimed*/ )
{
	++*static_cast<uint16_t*>(proc->state);
	value = static_cast<uint16_t>(value + kOffset);
}

static uint16_t s_reframes = 0u;

static CbProc s_reframeProcs[] = {
	{ .name = "offset", .fn = offsetFn, .cfg = nullptr, .state = &s_reframes },
};

static CbChain s_reframeChains[] = {
	{ .name = "reframe_ly", .inCh = AnalogComBusID::PS4_LY, .outCh = AnalogComBusID::PS4_LY,
	  .procs = s_reframeProcs, .procCount = 1u },
};


	/// The inputs of the dumper truck on this board: all of them.
static const AnalogComBusID kAllAnalogInputs[] = {
	AnalogComBusID::THROTTLE_STICK, AnalogComBusID::STEERING_STICK, AnalogComBusID::DUMP_STICK,
};
static const DigitalComBusID kAllDigitalInputs[] = {
	DigitalComBusID::SUBGEAR_SET_BTN,   DigitalComBusID::GEAR_UP_BTN,       DigitalComBusID::GEAR_DOWN_BTN,
	DigitalComBusID::CRUISE_UPDATE_BTN, DigitalComBusID::HORN_BTN,          DigitalComBusID::KEY_BTN,
	DigitalComBusID::DIRECT_DRIVE_BTN,  DigitalComBusID::CRUISE_TOGGLE_BTN,
};
static const RemoteInputs kAllInputs = {
	{ kAllAnalogInputs,  static_cast<uint8_t>(std::size(kAllAnalogInputs)) },
	{ kAllDigitalInputs, static_cast<uint8_t>(std::size(kAllDigitalInputs)) },
};


	/// The table of the type, for the fake source: LY to the throttle, circle to the horn.
static const RemoteAnalogRoute  kTypeAnalog[]  = { { AnalogComBusID::PS4_LY, AnalogComBusID::THROTTLE_STICK, false } };
static const RemoteDigitalRoute kTypeDigital[] = { { DigitalComBusID::PS4_CIRCLE, DigitalComBusID::HORN_BTN, false } };
static const RemoteMapTable     kTypeMap       = { "type", { kTypeAnalog, 1u }, { kTypeDigital, 1u } };

	/// The table of an instance: LY to the steering, inverted; circle to the key.
static const RemoteAnalogRoute  kInstAnalog[]  = { { AnalogComBusID::PS4_LY, AnalogComBusID::STEERING_STICK, true } };
static const RemoteDigitalRoute kInstDigital[] = { { DigitalComBusID::PS4_CIRCLE, DigitalComBusID::KEY_BTN, false } };
static const RemoteMapTable     kInstMap       = { "instance", { kInstAnalog, 1u }, { kInstDigital, 1u } };

	/// Refused: the same input written twice.
static const RemoteAnalogRoute  kBadAnalog[]   = {
	{ AnalogComBusID::PS4_LY, AnalogComBusID::THROTTLE_STICK, false },
	{ AnalogComBusID::PS4_RY, AnalogComBusID::THROTTLE_STICK, false },
};
static const RemoteMapTable     kBadMap        = { "bad", { kBadAnalog, 2u }, { nullptr, 0u } };


static constexpr uint32_t kPeriodMs = 20u;
static_assert(remote_period_ok(kPeriodMs), "the period of the fake module is over the ceiling");

static RemoteModuleCfg   s_modCfg;
static RemoteModuleState s_modState;
static RemoteModule      s_module = { &s_modCfg, &s_modState };


	/** A fresh module, with the given tables, not started. */
static void freshModule( const RemoteMapTable* typeMap, const RemoteMapTable* instanceMap )
{
	s_backend  = {};
	s_reframes = 0u;
	s_modState = {};
	s_modCfg   = {
		.name        = "fake_ps4",
		.backend     = { nullptr, fakeRefresh, &s_backend },
		.periodMs    = kPeriodMs,
		.reframer    = { s_reframeChains, static_cast<uint8_t>(std::size(s_reframeChains)) },
		.typeMap     = typeMap,
		.instanceMap = instanceMap,
		.inputs      = &kAllInputs,
	};
}


	/** Force the next poll to read. */
static void nextSample( void ) { s_modState.everRead = false; }



// =============================================================================
// MODULE ROWS
// =============================================================================

void test_m1_the_module_drives_through_the_base( void )
{
	freshModule(&kTypeMap, nullptr);
	TEST_ASSERT_TRUE(remote_module_begin(s_module));

	static const RemoteSourceDev devs[] = { { "fake_ps4", remote_module_poll, remote_module_apply, &s_module } };
	s_sources.devs  = devs;
	s_sources.count = 1u;

	s_backend.connected = true;
	s_backend.ly        = 40000u;
	s_backend.circle    = true;
	cycle();

	TEST_ASSERT_EQUAL(static_cast<int>(RemoteSourceState::ONE), static_cast<int>(s_sources.watch.state));
	TEST_ASSERT_EQUAL_UINT16_MESSAGE(40000u + kOffset, analog(AnalogComBusID::THROTTLE_STICK),
	                                 "the mapper must read the channel the reframer left");
	TEST_ASSERT_TRUE(digital(DigitalComBusID::HORN_BTN));
	TEST_ASSERT_FALSE(nodeLinkFail());

		// The controller goes away: safe values through the base
	s_backend.connected = false;
	nextSample();
	cycle();

	assertSafeValues();
	TEST_ASSERT_TRUE(nodeLinkFail());
}


void test_m2_the_table_of_the_instance_replaces_the_one_of_the_type( void )
{
	freshModule(&kTypeMap, &kInstMap);
	TEST_ASSERT_TRUE(remote_module_begin(s_module));
	TEST_ASSERT_EQUAL_STRING("instance", s_modState.map.name);

	s_backend.connected = true;
	s_backend.ly        = 20000u;
	s_backend.circle    = true;
	combus_set_digital(comBus, DigitalComBusID::KEY_BTN, false);

	TEST_ASSERT_TRUE(remote_module_poll(comBus, &s_module));
	remote_module_apply(comBus, &s_module);

	TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(comBus.analogBusMaxVal - (20000u + kOffset)),
	                         analog(AnalogComBusID::STEERING_STICK));
	TEST_ASSERT_EQUAL_UINT16_MESSAGE(1000u, analog(AnalogComBusID::THROTTLE_STICK),
	                                 "a row of the type must not survive: no fusion");
	TEST_ASSERT_TRUE(digital(DigitalComBusID::KEY_BTN));
	TEST_ASSERT_TRUE(digital(DigitalComBusID::HORN_BTN));   // untouched: still the value moveInputs() left
}


void test_m3_a_refused_table_at_boot_refuses_the_source( void )
{
	freshModule(&kBadMap, nullptr);
	TEST_ASSERT_FALSE(remote_module_begin(s_module));

	s_backend.connected = true;
	TEST_ASSERT_FALSE(remote_module_poll(comBus, &s_module));
	TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, s_backend.reads, "a refused source must not even be read");

		// No table at all is refused too
	freshModule(nullptr, nullptr);
	TEST_ASSERT_FALSE(remote_module_begin(s_module));
}


void test_m4_a_refused_table_at_the_door_leaves_the_previous_one( void )
{
	freshModule(&kTypeMap, nullptr);
	TEST_ASSERT_TRUE(remote_module_begin(s_module));

	TEST_ASSERT_FALSE(remote_module_map_apply(s_module, kBadMap));
	TEST_ASSERT_EQUAL_STRING("type", s_modState.map.name);
	TEST_ASSERT_EQUAL_UINT8(1u, s_modState.map.analogCount);
	TEST_ASSERT_TRUE(s_modState.mapReady);

	s_backend.connected = true;
	s_backend.ly        = 30000u;
	TEST_ASSERT_TRUE(remote_module_poll(comBus, &s_module));
	remote_module_apply(comBus, &s_module);
	TEST_ASSERT_EQUAL_UINT16(30000u + kOffset, analog(AnalogComBusID::THROTTLE_STICK));

		// A valid table through the door takes its place
	TEST_ASSERT_TRUE(remote_module_map_apply(s_module, kInstMap));
	TEST_ASSERT_EQUAL_STRING("instance", s_modState.map.name);
}


void test_m5_the_checks_of_a_table( void )
{
	RemoteMapRam ram = {};

		// A channel of the source read twice
	static const RemoteDigitalRoute twiceFrom[] = {
		{ DigitalComBusID::PS4_CIRCLE, DigitalComBusID::HORN_BTN, false },
		{ DigitalComBusID::PS4_CIRCLE, DigitalComBusID::KEY_BTN,  false },
	};
	TEST_ASSERT_FALSE(remote_map_apply(ram, { "twice_from", { nullptr, 0u }, { twiceFrom, 2u } }, kAllInputs));

		// An input written twice
	TEST_ASSERT_FALSE(remote_map_apply(ram, kBadMap, kAllInputs));

		// A channel that is not an input (an output of the machine)
	static const RemoteDigitalRoute notInput[] = {
		{ DigitalComBusID::PS4_CIRCLE, DigitalComBusID::NODE_LINK_FAIL, false },
	};
	TEST_ASSERT_FALSE(remote_map_apply(ram, { "not_input", { nullptr, 0u }, { notInput, 1u } }, kAllInputs));

		// A channel that is not on the bus
	static const RemoteAnalogRoute offBus[] = {
		{ static_cast<AnalogComBusID>(AnalogComBusID::CH_COUNT), AnalogComBusID::THROTTLE_STICK, false },
	};
	TEST_ASSERT_FALSE(remote_map_apply(ram, { "off_bus", { offBus, 1u }, { nullptr, 0u } }, kAllInputs));

		// Too long
	TEST_ASSERT_FALSE(remote_map_apply(ram, { "too_long", { kTypeAnalog, kRemoteMapMaxAnalog + 1u }, { nullptr, 0u } },
	                                   kAllInputs));

	TEST_ASSERT_NULL_MESSAGE(ram.name, "a refused table must leave nothing in RAM");
	TEST_ASSERT_EQUAL_UINT8(0u, ram.analogCount);
	TEST_ASSERT_EQUAL_UINT8(0u, ram.digitalCount);

		// And a valid one passes
	TEST_ASSERT_TRUE(remote_map_apply(ram, kTypeMap, kAllInputs));
}


void test_m6_the_reframer_runs_after_a_read_that_succeeded_only( void )
{
	freshModule(&kTypeMap, nullptr);
	TEST_ASSERT_TRUE(remote_module_begin(s_module));

		// Not connected: read, but no reframing
	TEST_ASSERT_FALSE(remote_module_poll(comBus, &s_module));
	TEST_ASSERT_EQUAL_UINT16(1u, s_backend.reads);
	TEST_ASSERT_EQUAL_UINT16(0u, s_reframes);

		// Connected: one reframing per read
	s_backend.connected = true;
	nextSample();
	TEST_ASSERT_TRUE(remote_module_poll(comBus, &s_module));
	TEST_ASSERT_EQUAL_UINT16(1u, s_reframes);

		// Within the period: no read, no reframing, the reframed value stays
	TEST_ASSERT_TRUE(remote_module_poll(comBus, &s_module));
	TEST_ASSERT_EQUAL_UINT16(2u, s_backend.reads);
	TEST_ASSERT_EQUAL_UINT16(1u, s_reframes);
	TEST_ASSERT_EQUAL_UINT16(CbusNeutral + kOffset, analog(AnalogComBusID::PS4_LY));
}


void test_m7_one_read_per_period( void )
{
	freshModule(&kTypeMap, nullptr);
	TEST_ASSERT_TRUE(remote_module_begin(s_module));
	s_backend.connected = true;

	remote_module_poll(comBus, &s_module);
	remote_module_poll(comBus, &s_module);
	remote_module_poll(comBus, &s_module);
	TEST_ASSERT_EQUAL_UINT16(1u, s_backend.reads);

	delay(kPeriodMs + 2u);
	remote_module_poll(comBus, &s_module);
	TEST_ASSERT_EQUAL_UINT16(2u, s_backend.reads);
}


void test_m8_the_table_of_the_type_for_the_ps4_passes_the_checks( void )
{
	RemoteMapRam ram = {};

	TEST_ASSERT_TRUE(remote_map_apply(ram, kPs4TypeMap, kAllInputs));
	TEST_ASSERT_EQUAL_UINT8(3u, ram.analogCount);
	TEST_ASSERT_EQUAL_UINT8(8u, ram.digitalCount);
	TEST_ASSERT_NULL_MESSAGE(ps4_instance_map(), "no instance declares a table in this build");
}


void test_m9_the_scaling_of_the_ps4_axes( void )
{
	const uint32_t maxV = comBus.analogBusMaxVal;

		// A stick: 0 at one end, neutral at the centre, max at the other end
	TEST_ASSERT_EQUAL_UINT16(0u,          ps4_scale_axis(-127, kPs4Stick, maxV));
	TEST_ASSERT_EQUAL_UINT16(CbusNeutral, ps4_scale_axis(0,    kPs4Stick, maxV));
	TEST_ASSERT_EQUAL_UINT16(65535u,      ps4_scale_axis(127,  kPs4Stick, maxV));
	TEST_ASSERT_EQUAL_UINT16(0u,          ps4_scale_axis(-128, kPs4Stick, maxV));   // clamped

		// A trigger: neutral at rest and in the deadband, max fully pressed
	TEST_ASSERT_EQUAL_UINT16(CbusNeutral, ps4_scale_axis(0,   kPs4Trigger, maxV));
	TEST_ASSERT_EQUAL_UINT16(CbusNeutral, ps4_scale_axis(5,   kPs4Trigger, maxV));
	TEST_ASSERT_TRUE(ps4_scale_axis(6, kPs4Trigger, maxV) > CbusNeutral);
	TEST_ASSERT_EQUAL_UINT16(65535u,      ps4_scale_axis(255, kPs4Trigger, maxV));
}


void test_m10_the_cost_of_a_read_with_a_reframer( void )
{
		// Six chains of one cb_scale each: one per axis of a controller
	static const CbScaleCfg kScale = { .inMax = 65535u, .outMax = 60000u };
	static CbProc  procs[6] = {};
	static CbChain chains[6] = {};
	const AnalogComBusID axes[6] = {
		AnalogComBusID::PS4_LX, AnalogComBusID::PS4_LY, AnalogComBusID::PS4_RX,
		AnalogComBusID::PS4_RY, AnalogComBusID::PS4_L2, AnalogComBusID::PS4_R2,
	};

	for (uint8_t i = 0u; i < 6u; ++i) {
		procs[i].name       = "scale";
		procs[i].fn         = cb_scale_fn;
		procs[i].cfg        = &kScale;
		procs[i].state      = nullptr;
		chains[i].name      = "reframe";
		chains[i].inCh      = axes[i];
		chains[i].outCh     = axes[i];
		chains[i].procs     = &procs[i];
		chains[i].procCount = 1u;
	}

	freshModule(&kPs4TypeMap, nullptr);
	s_modCfg.reframer = { chains, 6u };
	TEST_ASSERT_TRUE(remote_module_begin(s_module));
	s_backend.connected = true;

		// A read with its reframing, then the mapper, averaged
	constexpr uint16_t kPasses = 1000u;
	const uint32_t     t0      = micros();

	for (uint16_t n = 0u; n < kPasses; ++n) {
		nextSample();
		remote_module_poll(comBus, &s_module);
		remote_module_apply(comBus, &s_module);
	}

	const uint32_t dt = micros() - t0;

	char msg[96];
	snprintf(msg, sizeof(msg), "read + reframer (6 chains) + mapper (11 rows): %lu us per pass (%u passes)",
	         (unsigned long)(dt / kPasses), (unsigned)kPasses);
	TEST_MESSAGE(msg);

	TEST_ASSERT_EQUAL_UINT16(kPasses, s_backend.reads);
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

	RUN_TEST(test_m1_the_module_drives_through_the_base);
	RUN_TEST(test_m2_the_table_of_the_instance_replaces_the_one_of_the_type);
	RUN_TEST(test_m3_a_refused_table_at_boot_refuses_the_source);
	RUN_TEST(test_m4_a_refused_table_at_the_door_leaves_the_previous_one);
	RUN_TEST(test_m5_the_checks_of_a_table);
	RUN_TEST(test_m6_the_reframer_runs_after_a_read_that_succeeded_only);
	RUN_TEST(test_m7_one_read_per_period);
	RUN_TEST(test_m8_the_table_of_the_type_for_the_ps4_passes_the_checks);
	RUN_TEST(test_m9_the_scaling_of_the_ps4_axes);
	RUN_TEST(test_m10_the_cost_of_a_read_with_a_reframer);

	UNITY_END();
}

void loop() {}

// EOF test_remote_source.cpp
