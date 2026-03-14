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


// Each machine config wraps its channel enums (AnalogComBusID, DigitalComBusID)
// inside a dedicated namespace (e.g. DumperTruck::, AutreMachine::).
// This allows a remote build to include multiple machine configs simultaneously
// without name collisions.
//
// On machine-side builds, `using namespace <Machine>` below re-exports the
// enums into the global scope so all existing code continues to compile
// unchanged (no prefix needed).
//
// On remote builds, the `using namespace` directive is NOT present — the
// remote always uses the fully qualified name (DumperTruck::AnalogComBusID::...)
// to explicitly identify which machine's channel is being accessed.

#if defined(MACHINE_TYPE) && (MACHINE_TYPE == DUMPER_TRUCK)
  #include "dumper_truck/dumper_truck.h"
  using namespace DumperTruck;

#elif defined(MACHINE_TYPE) && (MACHINE_TYPE == AUTRE_MACHINE_EXEMPLE)
  #include "autre_machine/autre_machine.h"
  // using namespace AutreMachine;   // uncomment when namespace is added

#else
    #error "No machine type defined for com-bus. Check platformio.ini file and env:xxx setting to fix the problem"
#endif


// EOF combus_types.h