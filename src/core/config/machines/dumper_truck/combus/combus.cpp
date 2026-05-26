
#include "combus.h"
#include <core/system/combus/combus_res.h>  // CbusNeutral

#ifndef IS_REMOTE

using namespace DumperTruck;


// =============================================================================
// COM-BUS CHANNEL ARRAYS  (single-combus nodes)
// =============================================================================

AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)] = {
  // --- Wire channels (0 .. WIRE_END-1) --- transmitted over UART ---
  { .infoName = "steering channel",        .value = CbusNeutral, .owner = ChanOwner::MACHINE_INPUT  },
  { .infoName = "rpm channel",              .value = 0u,          .owner = ChanOwner::MACHINE_SYSTEM }, ///< Engine RPM — written by SIM_RPM pipeline, transmitted to sound node.
  { .infoName = "dump actuators channel",  .value = CbusNeutral, .owner = ChanOwner::MACHINE_INPUT  },
  { .infoName = "esc speed channel",       .value = CbusNeutral, .owner = ChanOwner::MACHINE_SYSTEM },
  { .infoName = "gear channel",            .value = 1u,          .owner = ChanOwner::MACHINE_SYSTEM }, ///< Virtual gear (1–3) from motion pipeline; direction in ESC_REVERSE.
  { .infoName = "drive state channel",     .value = 0u,          .owner = ChanOwner::MACHINE_SYSTEM }, ///< DriveState 3-bit bitmask — see DriveStateBus::encode() in simulation_struct.h.
  // --- Machine-node-local channels (WIRE_END .. CH_COUNT-1) — never transmitted ---
  { .infoName = "analog brake channel",    .value = 0u         , .owner = ChanOwner::MACHINE_INPUT  }, ///< L2 trigger input — written by input_manager, read by sim_brake.
  { .infoName = "sub-gear channel",        .value = 0u,          .owner = ChanOwner::MACHINE_SYSTEM }, ///< Active sub-gear index — written by sim_gear.
  { .infoName = "dump ramp channel",       .value = CbusNeutral, .owner = ChanOwner::MACHINE_SYSTEM }, ///< Inertia-smoothed dump position — written by sim_ramp, read by DUMP_ACTUATOR.
  { .infoName = "steering ramp channel",   .value = CbusNeutral, .owner = ChanOwner::MACHINE_SYSTEM }, ///< Inertia-smoothed steering position — written by sim_ramp, read by STEERING.
  { .infoName = "throttle channel",        .value = CbusNeutral, .owner = ChanOwner::MACHINE_INPUT  }, ///< Throttle stick — written by input_manager, read by SIM_THROTTLE.
};



DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)] = {
  // --- Wire channels (0 .. WIRE_END-1) — transmitted over UART ---
  { .infoName = "horn channel",            .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "lights channel",          .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "key channel",             .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "battery low channel",     .value = false, .owner = ChanOwner::VBAT_MON      },
  { .infoName = "indicator left channel",  .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "indicator right channel", .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "hazards channel",         .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "high beam channel",       .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "roof light channel",      .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "low beam channel",        .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "braking channel",         .value = false, .owner = ChanOwner::MACHINE_SYSTEM },
  // --- Machine-node-local channels (WIRE_END .. MACHINE_END-1) — never transmitted ---
  { .infoName = "direct drive (local)",    .value = false, .owner = ChanOwner::MACHINE_SYSTEM }, ///< Toggle state — written by main.cpp toggle logic, read by cb_bypass_fn.
  { .infoName = "direct drive btn (local)", .value = false, .owner = ChanOwner::MACHINE_INPUT }, ///< Raw OPTIONS button — written by input_update every cycle.
  { .infoName = "manual gear set (local)", .value = false, .owner = ChanOwner::MACHINE_SYSTEM }, ///< Manual gear mode active — written by INPUT chain, read by sim_manual_gear_fn.
  { .infoName = "subgear set btn (local)", .value = false, .owner = ChanOwner::MACHINE_INPUT  }, ///< Raw sub-gear toggle button — written by input_update; processed by ctrl chain.
  { .infoName = "subgear up btn (local)",  .value = false, .owner = ChanOwner::MACHINE_INPUT  }, ///< Raw sub-gear up button — written by input_update; read by INPUT chain.
  { .infoName = "subgear down btn (local)", .value = false, .owner = ChanOwner::MACHINE_INPUT  }, ///< Raw sub-gear down button — written by input_update; read by INPUT chain.
  // --- Sound-node-local channels (MACHINE_END .. CH_COUNT-1) — never transmitted ---
  { .infoName = "always on (local)",       .value = true,  .owner = ChanOwner::NONE }, ///< Stays true: continuous sources driven by volMod.
  { .infoName = "siren (local)",           .value = false, .owner = ChanOwner::NONE }, ///< Siren/cannon mode toggle.
  { .infoName = "indicator tick (local)",  .value = false, .owner = ChanOwner::NONE }, ///< Combined indicator gate.
  { .infoName = "esc reverse (local)",     .value = false, .owner = ChanOwner::NONE }, ///< ESC reverse state.
  { .infoName = "sound1 (local)",          .value = false, .owner = ChanOwner::NONE }, ///< One-shot, cleared by EffectsGen.
  { .infoName = "wastegate (local)",       .value = false, .owner = ChanOwner::NONE }, ///< One-shot, cleared by EffectsGen.
  { .infoName = "brake (local)",           .value = false, .owner = ChanOwner::NONE }, ///< Resolved by EffectsGen (EngineGen||sim).
  { .infoName = "parking brake (local)",   .value = false, .owner = ChanOwner::NONE }, ///< EngineGen level flag.
  { .infoName = "shifting (local)",        .value = false, .owner = ChanOwner::NONE }, ///< One-shot, cleared by EffectsGen.
  { .infoName = "coupling (local)",        .value = false, .owner = ChanOwner::NONE }, ///< One-shot, cleared by EffectsGen.
  { .infoName = "uncoupling (local)",      .value = false, .owner = ChanOwner::NONE }, ///< One-shot, cleared by EffectsGen.
  { .infoName = "bucket rattle (local)",   .value = false, .owner = ChanOwner::NONE }, ///< One-shot, cleared by EffectsGen.
  { .infoName = "out of fuel (local)",     .value = false, .owner = ChanOwner::NONE }, ///< One-shot, cleared by EffectsGen.
};



/**
 * @brief Communication bus structure definition.
 *
 * @details EnvCfg-perspective owners — shared across all nodes.
 *   Foreign nodes (sound, remote) may only write via the UART RX bridge.
 */

ComBus comBus {
  .runLevel       = RunLevel::NOT_YET_SET,
  .runLevelOwner  = ChanOwner::MACHINE_SYSTEM,
  .keyOnOwner     = ChanOwner::MACHINE_SYSTEM,
  .analogBus      = AnalogComBusArray,
  .digitalBus     = DigitalComBusArray,
  .analogBusMaxVal = (1UL << (sizeof(decltype(AnalogComBus::value)) * 8)) - 1
};

#endif // !IS_REMOTE
