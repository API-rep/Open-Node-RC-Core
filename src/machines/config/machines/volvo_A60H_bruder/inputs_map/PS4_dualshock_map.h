#include <core/config/inputs/inputs.h>              // device vocabulary + InputAnalogMap, InputDigitalMap
#include <machines/config/machines/volvo_A60H_bruder/combus/combus.h>  // AnalogComBusID, DigitalComBusID

  // input to combus analog channel mapping
extern const InputAnalogMap InputAnalogMapArray[];

  // number of analog mappings in InputAnalogMap array
extern const uint8_t InputAnalogMapCount;

  // input to combus digital channel mapping
extern const InputDigitalMap InputDigitalMapArray[];

  // number of digital mappings in InputDigital Map array
extern const uint8_t InputDigitalMapCount;
