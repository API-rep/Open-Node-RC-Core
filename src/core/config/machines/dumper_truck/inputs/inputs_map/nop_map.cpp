/*!****************************************************************************
 * @file  nop_map.cpp
 * @brief Empty input mapping definition for INPUT_NONE.
 *******************************************************************************/

#include "nop_map.h"

  // zero-length arrays — linker must accept declarations of 0 entries.
  // In C++ this is well-defined (zero-size arrays are allowed since C++14
  // when declared as `extern T arr[0]`), but the link symbols must still
  // resolve. We provide them as plain non-const-static values at TU scope.
const InputAnalogMap  InputAnalogMapArray[0]  = {};
const uint8_t InputAnalogMapCount = 0;

const InputDigitalMap InputDigitalMapArray[0] = {};
const uint8_t InputDigitalMapCount = 0;


// EOF nop_map.cpp
