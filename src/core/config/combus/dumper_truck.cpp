
#include "dumper_truck.h"

AnalogBus analogBusArray[COMBUS_ANALOG_CH_COUNT] = {
  { .infoName = "steering channel" },
  { .infoName = "driving speed channel" },
  { .infoName = "dump actuators channel" },
};



DigitalBus digitalBusArray[COMBUS_DIGITAL_CH_COUNT] = {
  
  { .infoName = "horn channel" },
  { .infoName = "lights channel" },
};



/**
 * @brief Communication bus structure definition
 */

ComBus comBus {
  .runLevel = RunLevel::NOT_YET_SET,
  .analogBus = analogBusArray,
  .digitalBus = digitalBusArray,
  .analogBusMaxVal = (1UL << (sizeof(decltype(AnalogBus::value)) * 8)) - 1
};