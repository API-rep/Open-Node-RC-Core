/*!****************************************************************************
 * @file  combus_types.h
 * @brief Com-bus full umbrella (machine builds)
 *
 * Dispatches to the machine-specific com-bus file which, when IS_MACHINE is
 * defined, exposes enum IDs, array externs, comBus extern and input mapping.
 *
 * All consumers (machine node, sound node, remote node) include this file.
 * The IS_MACHINE guard inside each machine file controls what is exposed:
 *   - IS_MACHINE absent  → enum IDs only (AnalogComBusID, DigitalComBusID)
 *   - IS_MACHINE defined → enum IDs + externs + inputs_map
 *
 * NOTE: when adding a new machine type, add one #elif branch below.
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


// EOF combus_types.h