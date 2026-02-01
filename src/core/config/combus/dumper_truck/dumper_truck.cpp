
#include "dumper_truck.h"

AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)] = {
  { .infoName = "steering channel" },
  { .infoName = "driving speed channel" },
  { .infoName = "dump actuators channel" },
};



DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)] = {
  
  { .infoName = "horn channel" },
  { .infoName = "lights channel" },
};



/**
 * @brief Communication bus structure definition
 */

ComBus comBus {
  .runLevel = RunLevel::NOT_YET_SET,
  .analogBus = AnalogComBusArray,
  .digitalBus = DigitalComBusArray,
  .analogBusMaxVal = (1UL << (sizeof(decltype(AnalogComBus::value)) * 8)) - 1
};