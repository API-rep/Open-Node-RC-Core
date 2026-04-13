/*!****************************************************************************
 * @file  combus_types.h
 * @brief Com-bus full umbrella — enum IDs
 *
 * Dispatches to the machine-specific com-bus file which exposes, depending on
 * the build environment:
 *   - IS_REMOTE defined   → enum IDs only (prefixed: DumperTruck::AnalogComBusID::…)
 *   - Single-combus nodes → enum IDs + array externs + comBus extern (i.e. machine nodes)
 *   - INPUT_MODULE set    → also includes inputs_map.h (input-device → com-bus mapping)
 *
 * NAMESPACE STRATEGY:
 *   Each machine wraps its channel enums inside a dedicated namespace
 *   (e.g. namespace DumperTruck) so that a remote build can include multiple
 *   machine configs in the same TU without name collisions.
 * 
 *   - On machine-side builds, `using namespace <Machine>` re-exports the enums
 *   into global scope. All existing code compiles unchanged, no prefix needed.
 * 
 *   - On remote builds the directive is absent; the remote uses the fully
 *   qualified form (DumperTruck::AnalogComBusID::...) to distinguish machines.
 *   The enum values themselves (IDs only, no deps) live in <name>_ids.h and are
 *   dispatched by combus_ids.h, which struct headers include to break the
 *   include cycle with combus_struct.h.
 *
 * NOTE: when adding a new machine type, add one #elif branch below AND one
 *       matching branch in combus_ids.h.
 *******************************************************************************///
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

#if defined(MACHINE_TYPE) && (MACHINE_TYPE == DUMPER_TRUCK)
  #include <core/config/machines/dumper_truck/combus/dumper_truck.h>
  using namespace DumperTruck;

#elif defined(MACHINE_TYPE) && (MACHINE_TYPE == AUTRE_MACHINE_EXEMPLE)
  #include <core/config/machines/autre_machine/combus/autre_machine.h>
  // using namespace AutreMachine;   // uncomment when namespace is added

#else
    #error "No machine type defined for com-bus. Check platformio.ini file and env:xxx setting to fix the problem"
#endif


// EOF combus_types.h