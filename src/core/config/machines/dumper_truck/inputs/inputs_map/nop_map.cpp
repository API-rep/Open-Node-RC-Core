/*!****************************************************************************
 * @file  nop_map.cpp
 * @brief Empty input mapping definition for INPUT_NONE.
 *******************************************************************************/

#include "nop_map.h"

// This TU is one half of the input-mapping dispatch. The PS4 mapping
// (`PS4_dualshock_map.cpp`) defines the same four symbols when the
// INPUT_PS4_DS4_BT flag is set. Compiling both TUs together yields a
// linker `multiple definition` error on those four symbols.
//
// The matching guard is in `PS4_dualshock_map.cpp` (complementary
// #if defined(INPUT_PS4_DS4_BT)). PlatformIO compiles every .cpp in
// the active build_src_filter, so the guard has to be in the .cpp
// itself, not just in a .h (which would only suppress re-inclusion
// of the declarations). This mirrors the `inputs.h` dispatch at the
// header level.
#if defined(INPUT_NONE)

  // zero-length arrays — linker must accept declarations of 0 entries.
  // In C++ this is well-defined (zero-size arrays are allowed since C++14
  // when declared as `extern T arr[0]`), but the link symbols must still
  // resolve. We provide them as plain non-const-static values at TU scope.
const InputAnalogMap  InputAnalogMapArray[0]  = {};
const uint8_t InputAnalogMapCount = 0;

const InputDigitalMap InputDigitalMapArray[0] = {};
const uint8_t InputDigitalMapCount = 0;

#endif  // INPUT_NONE


// EOF nop_map.cpp
