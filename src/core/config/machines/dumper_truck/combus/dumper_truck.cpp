
#include "dumper_truck.h"

#ifndef IS_REMOTE

using namespace DumperTruck;


// =============================================================================
// COM-BUS CHANNEL ARRAYS  (single-combus nodes)
// =============================================================================

AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)] = {
#ifdef SOUND_NODE
    // On the sound node all analog channels are written by the UART RX bridge.
  { .infoName = "steering channel",        .owner = ChanOwner::SYSTEM_EXT },
  { .infoName = "driving speed channel",   .owner = ChanOwner::SYSTEM_EXT },
  { .infoName = "dump actuators channel",  .owner = ChanOwner::SYSTEM_EXT },
  { .infoName = "esc speed channel",       .owner = ChanOwner::SYSTEM_EXT },
#else
  { .infoName = "steering channel",        .owner = ChanOwner::INPUT_DEV },
  { .infoName = "driving speed channel",   .owner = ChanOwner::INPUT_DEV },
  { .infoName = "dump actuators channel",  .owner = ChanOwner::INPUT_DEV },
  { .infoName = "esc speed channel",       .owner = ChanOwner::SYSTEM    },
#endif
};



DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)] = {
#ifdef SOUND_NODE
    // On the sound node all digital channels are written by the UART RX bridge,
    // except BATTERY_LOW which is written locally by the vbat module.
  { .infoName = "horn channel",            .owner = ChanOwner::SYSTEM_EXT },
  { .infoName = "lights channel",          .owner = ChanOwner::SYSTEM_EXT },
  { .infoName = "key channel",             .owner = ChanOwner::SYSTEM_EXT },
  { .infoName = "battery low channel",     .owner = ChanOwner::VBAT_MON  },
  { .infoName = "indicator left channel",  .owner = ChanOwner::SYSTEM_EXT },
  { .infoName = "indicator right channel", .owner = ChanOwner::SYSTEM_EXT },
  { .infoName = "hazards channel",         .owner = ChanOwner::SYSTEM_EXT },
#else
  { .infoName = "horn channel",            .owner = ChanOwner::INPUT_DEV },
  { .infoName = "lights channel",          .owner = ChanOwner::INPUT_DEV },
  { .infoName = "key channel",             .owner = ChanOwner::INPUT_DEV },
  { .infoName = "battery low channel",     .owner = ChanOwner::VBAT_MON },
  { .infoName = "indicator left channel",  .owner = ChanOwner::INPUT_DEV },
  { .infoName = "indicator right channel", .owner = ChanOwner::INPUT_DEV },
  { .infoName = "hazards channel",         .owner = ChanOwner::INPUT_DEV },
#endif
};



/**
 * @brief Communication bus structure definition.
 *
 * @details Ownership fields define the default writer mandate for each
 *   meta-channel.  The writing module verifies its own access right at
 *   runtime — ownership is the same for all mono-combus nodes.
 */

ComBus comBus {
  .runLevel       = RunLevel::NOT_YET_SET,
#ifdef SOUND_NODE
  .runLevelOwner  = ChanOwner::SYSTEM_EXT,    // UART RX bridge writes runLevel
  .keyOnOwner     = ChanOwner::SYSTEM_EXT,    // UART RX bridge writes keyOn
#else
  .runLevelOwner  = ChanOwner::SYSTEM,        // system layer (RunLevel FSM)
  .keyOnOwner     = ChanOwner::SYSTEM,        // system layer (key-on logic from input)
#endif
  .analogBus      = AnalogComBusArray,
  .digitalBus     = DigitalComBusArray,
  .analogBusMaxVal = (1UL << (sizeof(decltype(AnalogComBus::value)) * 8)) - 1
};

#endif // !IS_REMOTE