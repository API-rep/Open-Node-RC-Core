#include "combus.h"
#include <core/system/combus/combus_res.h>  // CbusNeutral

// =============================================================================
// COM-BUS CHANNEL ARRAYS
// =============================================================================
//
// A9.9: raw input acquisition (SYSTEM) moved from combus_channels_local_*.inc
// to core/.../inputs/input_channels_*.inc \u2014 see combus.h includes (assembled
// into the enclosing scope as extra enum members via combus_ids.h).

AnalogComBus AnalogComBusArray[static_cast<uint8_t>(AnalogComBusID::CH_COUNT)] = {
  #include <core/config/machines/dumper_truck/combus/combus_channels_remote_analog.inc>
  #include "combus_channels_local_analog.inc"
  #include "combus_channels_system_analog.inc"
  #include <core/config/machines/dumper_truck/inputs/input_channels_analog.inc>
};

DigitalComBus DigitalComBusArray[static_cast<uint8_t>(DigitalComBusID::CH_COUNT)] = {
  #include <core/config/machines/dumper_truck/combus/combus_channels_remote_digital.inc>
  #include "combus_channels_local_digital.inc"
  #include "combus_channels_system_digital.inc"
  #include <core/config/machines/dumper_truck/inputs/input_channels_digital.inc>
};

// --- Garde-fous compile-time : cohérence enum <-> tableau ---
static_assert(
  static_cast<uint8_t>(AnalogComBusID::CH_COUNT) ==
  sizeof(AnalogComBusArray) / sizeof(AnalogComBusArray[0]),
  "AnalogComBusArray size mismatch — check combus_channels_*_analog.inc files"
);
static_assert(
  static_cast<uint8_t>(DigitalComBusID::CH_COUNT) ==
  sizeof(DigitalComBusArray) / sizeof(DigitalComBusArray[0]),
  "DigitalComBusArray size mismatch — check combus_channels_*_digital.inc files"
);

/**
 * @brief Communication bus structure definition — Volvo A60H Bruder.
 */
ComBus comBus {
  .runLevel        = RunLevel::NOT_YET_SET,
  .runLevelLayer   = ChanLayer::LOCAL,
  .analogBus       = AnalogComBusArray,
  .digitalBus      = DigitalComBusArray,
  .analogBusMaxVal = (1UL << (sizeof(decltype(AnalogComBus::value)) * 8)) - 1
};

// EOF combus.cpp