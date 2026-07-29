/*!****************************************************************************
 * @file  drivers.h
 * @brief DC driver dispatcher.
 *
 * Selects the DC driver backend configured for the current build.
 * The selected backend exports a common alias (`dcDriverModel`) used by the
 * rest of the project. Model is set via compile flag or platformio.ini file.
 * (e.g. -D BOARD_DC_DRIVER_DRV8801, -D BOARD_DC_DRIVER_DRV8874).
 * 
 * Hybrid boards may bypass this alias and reference backend models directly
 * when different driver models coexist on the same PCB.
 ******************************************************************************/
#pragma once

#if defined(BOARD_DC_DRIVER_DRV8801)
  #include "DRV8801.h"
  inline constexpr const DriverModel* boardDcDrivers = &DRV8801;

#elif defined(BOARD_DC_DRIVER_DRV8874)
  #include "DRV8874.h"
  inline constexpr const DriverModel* boardDcDrivers = &DRV8874;

#else
  #error "No DC driver backend selected."
#endif

// EOF drivers.h