/******************************************************************************
 * @file  input_sources.cpp
 * @brief The command sources of a machine board: its modules, run by the base.
 *****************************************************************************/

#include "input_sources.h"

#if defined(HAS_REMOTE_PS4)

#include <iterator>   // std::size

#include <combus_peer_links.h>                                    // generated: kRemoteAnalogInputs, kRemoteDigitalInputs
#include <core/system/input/remote_source.h>                      // remote_sources_update()
#include <core/system/input/remote_module.h>                      // RemoteModule
#include <core/system/input/ps4/ps4_backend.h>                    // ps4_backend_begin(), ps4_backend_refresh()
#include <core/config/machines/dumper_truck/inputs/ps4_map.h>     // kPs4TypeMap, ps4_instance_map()



// =============================================================================
// 1. THE SOURCES OF THE BOARD
// =============================================================================

	/// The inputs of the type on this board: what a mapper may write, what the base takes back.
static const RemoteInputs kInputs = {
	{ combus::peer::kRemoteAnalogInputs,  combus::peer::kRemoteAnalogInputCount },
	{ combus::peer::kRemoteDigitalInputs, combus::peer::kRemoteDigitalInputCount },
};


	/// The PS4 controller: one read every 10 ms (100 Hz, the ceiling), no reframing yet.
static constexpr uint32_t kPs4PeriodMs = 10u;
static_assert(remote_period_ok(kPs4PeriodMs), "PS4 module: read period over kRemoteSourceMaxHz");

#if defined(PS4_BLUETOOTH_ADDRESS)
static const char kPs4Address[] = PS4_BLUETOOTH_ADDRESS;
#endif

static RemoteModuleCfg s_ps4Cfg = {
	.name        = "ps4",
#if defined(PS4_BLUETOOTH_ADDRESS)
	.backend     = { ps4_backend_begin, ps4_backend_refresh, const_cast<char*>(kPs4Address) },
#else
	.backend     = { ps4_backend_begin, ps4_backend_refresh, nullptr },
#endif
	.periodMs    = kPs4PeriodMs,
	.reframer    = { nullptr, 0u },
	.typeMap     = &kPs4TypeMap,
	.instanceMap = nullptr,          // set at boot: ps4_instance_map()
	.inputs      = &kInputs,
};

static RemoteModuleState s_ps4State;
static RemoteModule      s_ps4 = { &s_ps4Cfg, &s_ps4State };


static const RemoteSourceDev kSources[] = {
	{ "ps4", remote_module_poll, remote_module_apply, &s_ps4 },
};

static RemoteSources s_sources = {
	.devs   = kSources,
	.count  = static_cast<uint8_t>(std::size(kSources)),
	.inputs = kInputs,
};



// =============================================================================
// 2. API
// =============================================================================

	/// The table of the instance, if it declares one, is known at link time only.
void input_sources_begin( void )
{
	s_ps4Cfg.instanceMap = ps4_instance_map();

	remote_module_begin(s_ps4);
}


void input_sources_update( ComBus& bus )
{
	remote_sources_update(s_sources, bus);
}


#else  // HAS_REMOTE_PS4

	// No source module on this board: nothing to start, nothing to run.
void input_sources_begin( void ) {}

void input_sources_update( ComBus& /*bus*/ ) {}

#endif  // HAS_REMOTE_PS4

// EOF input_sources.cpp
