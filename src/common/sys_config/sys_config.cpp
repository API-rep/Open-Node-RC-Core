
#include "sys_config.h"

AnalogBus analogBusArray[] = {

    // steering channel
  { 
    .infoName = "steering channel"
  },
  
    // driving speed channel
  { 
    .infoName = "driving speed channel"
  },

    // dump actuators channel
  { 
    .infoName = "dump actuators channel"
  },
};



DigitalBus digitalBusArray[] = {
  
    // steering channel
  { 
    .infoName = "horn channel"
  },
  
    // driving speed channel
  { 
    .infoName = "lights channel"
  },
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