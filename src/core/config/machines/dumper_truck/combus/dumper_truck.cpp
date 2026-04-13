
#include "dumper_truck.h"

#ifndef IS_REMOTE

using namespace DumperTruck;


// =============================================================================
// COM-BUS CHANNEL ARRAYS  (single-combus nodes)
// =============================================================================

AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)] = {
  { .infoName = "steering channel",        .owner = ChanOwner::INPUT_DEV },
  { .infoName = "driving speed channel",   .owner = ChanOwner::INPUT_DEV },
  { .infoName = "dump actuators channel",  .owner = ChanOwner::INPUT_DEV },
  { .infoName = "esc speed channel",       .owner = ChanOwner::SYSTEM    },
};



DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)] = {
  { .infoName = "horn channel",            .owner = ChanOwner::INPUT_DEV },
  { .infoName = "lights channel",          .owner = ChanOwner::INPUT_DEV },
  { .infoName = "key channel",             .owner = ChanOwner::INPUT_DEV },
  { .infoName = "battery low channel",     .owner = ChanOwner::VBAT_MON },
  { .infoName = "indicator left channel",  .owner = ChanOwner::INPUT_DEV },
  { .infoName = "indicator right channel", .owner = ChanOwner::INPUT_DEV },
  { .infoName = "hazards channel",         .owner = ChanOwner::INPUT_DEV },
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
  .runLevelOwner  = ChanOwner::SYSTEM,        // system layer (RunLevel FSM)
  .keyOnOwner     = ChanOwner::SYSTEM,        // system layer (key-on logic from input)
  .analogBus      = AnalogComBusArray,
  .digitalBus     = DigitalComBusArray,
  .analogBusMaxVal = (1UL << (sizeof(decltype(AnalogComBus::value)) * 8)) - 1
};

#endif // !IS_REMOTE