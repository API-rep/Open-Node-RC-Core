
#include "dumper_truck.h"

#ifndef IS_REMOTE

using namespace DumperTruck;


// =============================================================================
// COM-BUS CHANNEL ARRAYS  (single-combus nodes)
// =============================================================================

AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)] = {
  // --- Wire channels (0 .. WIRE_END-1) --- transmitted over UART ---
  { .infoName = "steering channel",        .owner = ChanOwner::MACHINE_INPUT  },
  { .infoName = "driving speed channel",   .owner = ChanOwner::MACHINE_INPUT  },
  { .infoName = "dump actuators channel",  .owner = ChanOwner::MACHINE_INPUT  },
  { .infoName = "esc speed channel",       .owner = ChanOwner::MACHINE_SYSTEM },
  { .infoName = "gear channel",            .owner = ChanOwner::MACHINE_SYSTEM }, ///< Virtual gear (1–3) from motion pipeline; direction in ESC_REVERSE.
};



DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)] = {
  // --- Wire channels (0 .. WIRE_END-1) — transmitted over UART ---
  { .infoName = "horn channel",            .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "lights channel",          .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "key channel",             .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "battery low channel",     .value = false, .owner = ChanOwner::VBAT_MON     },
  { .infoName = "indicator left channel",  .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "indicator right channel", .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "hazards channel",         .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "high beam channel",       .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "roof light channel",      .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "low beam channel",        .value = false, .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "braking channel",         .value = false, .owner = ChanOwner::MACHINE_SYSTEM },
  // --- Sound-node-local channels (WIRE_END .. CH_COUNT-1) — never transmitted ---
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
