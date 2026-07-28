/*!****************************************************************************
 * @file  nop.cpp
 * @brief No-input backend implementation.
 *
 * @details Defines the empty descriptor arrays declared in nop.h.
 *   These arrays are zero-sized (COUNT = 0), so no initialisers are needed.
 *******************************************************************************/
#if defined(INPUT_MODULE_NONE)

  #include "nop.h"

  AnalogInputDev AnalogInputDevArray[static_cast<uint8_t>(AnalogInputDevID::ANALOG_DEV_COUNT)] = {
  };

  DigitalInputDev digitalInputDevArray[static_cast<uint8_t>(DigitalInputDevID::DIGITAL_DEV_COUNT)] = {
  };

#endif

// EOF nop.cpp
