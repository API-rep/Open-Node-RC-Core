/*!****************************************************************************
 * @file  nop_map.h
 * @brief Empty input mapping for INPUT_NONE — autonomous / headless mode.
 *
 * @details When INPUT_NONE is defined, there is no input device. The mapping
 *   arrays are defined as empty (0 entries) so input_update() (if reached)
 *   has nothing to iterate over. The failsafe path keeps the bus at neutral
 *   since input_is_connected() returns false.
 *
 *   A9.9: lives in core/ alongside the PS4 mapping for consistency.
 *******************************************************************************///
#pragma once

#include <core/config/inputs/inputs.h>   // InputAnalogMap / InputDigitalMap structs

  // empty analog mapping
extern const InputAnalogMap InputAnalogMapArray[0];
extern const uint8_t InputAnalogMapCount;   // == 0

  // empty digital mapping
extern const InputDigitalMap InputDigitalMapArray[0];
extern const uint8_t InputDigitalMapCount;  // == 0


// EOF nop_map.h
