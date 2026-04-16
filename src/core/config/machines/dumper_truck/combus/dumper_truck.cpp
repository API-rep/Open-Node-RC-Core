
#include "dumper_truck.h"

#ifndef IS_REMOTE

using namespace DumperTruck;


// =============================================================================
// COM-BUS CHANNEL ARRAYS  (single-combus nodes)
// =============================================================================

AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)] = {
  { .infoName = "steering channel",        .owner = ChanOwner::MACHINE_INPUT  },
  { .infoName = "driving speed channel",   .owner = ChanOwner::MACHINE_INPUT  },
  { .infoName = "dump actuators channel",  .owner = ChanOwner::MACHINE_INPUT  },
  { .infoName = "esc speed channel",       .owner = ChanOwner::MACHINE_SYSTEM },
};



DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)] = {
  { .infoName = "horn channel",            .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "lights channel",          .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "key channel",             .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "battery low channel",     .owner = ChanOwner::VBAT_MON     },
  { .infoName = "indicator left channel",  .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "indicator right channel", .owner = ChanOwner::MACHINE_INPUT },
  { .infoName = "hazards channel",         .owner = ChanOwner::MACHINE_INPUT },
};



/**
 * @brief Communication bus structure definition.
 *
 * @details Machine-perspective owners — shared across all nodes.
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