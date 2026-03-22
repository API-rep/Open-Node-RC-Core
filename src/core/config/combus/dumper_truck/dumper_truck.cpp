
#include "dumper_truck.h"

AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)] = {
  { .infoName = "steering channel",        .owner = ChanOwner::INPUT_DEV },
  { .infoName = "driving speed channel",   .owner = ChanOwner::INPUT_DEV },
  { .infoName = "dump actuators channel",  .owner = ChanOwner::INPUT_DEV },
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
 * @brief Communication bus structure definition
 */

ComBus comBus {
  .runLevel       = RunLevel::NOT_YET_SET,
  .runLevelOwner  = ChanOwner::MACHINE,     // machine main.cpp FSM is the sole writer
  .battLowOwner   = ChanOwner::VBAT_MON,    // vbat_sense module writes batteryIsLow
  .keyOnOwner     = ChanOwner::REMOTE,      // written by the remote input module
  .analogBus      = AnalogComBusArray,
  .digitalBus     = DigitalComBusArray,
  .analogBusMaxVal = (1UL << (sizeof(decltype(AnalogComBus::value)) * 8)) - 1
};