/*!****************************************************************************
 * @file  combus.h
 * @brief Com-bus full umbrella (machine builds)
 *
 * Dispatches to the machine-specific com-bus file which, when IS_MACHINE is
 * defined, exposes enum IDs, array externs, comBus extern and input mapping.
 *
 * Cross-boundary consumers that need only the enum IDs (sound node, etc.)
 * should include combus_types.h instead — it dispatches to the same files
 * but the IS_MACHINE guard keeps structs and input headers out.
 *
 * NOTE: when adding a new machine type, add one #elif branch below AND the
 * matching branch in combus_types.h.
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>


#if defined(MACHINE_TYPE) && (MACHINE_TYPE == DUMPER_TRUCK)
  #include "dumper_truck/dumper_truck.h"

#elif defined(MACHINE_TYPE) && (MACHINE_TYPE == AUTRE_MACHINE_EXEMPLE)
  #include "autre_machine/autre_machine.h"

#else
    #error "No machine type defined for com-bus. Check platformio.ini file and env:xxx setting to fix the problem"
#endif


// EOF combus.h