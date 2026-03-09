/*!****************************************************************************
 * @file  combus_types.h
 * @brief Com-bus enum-only umbrella (cross-boundary consumers)
 *
 * Lightweight include path for consumers that need only AnalogComBusID /
 * DigitalComBusID without pulling in machine structs or input-mapping headers
 * (e.g. sound node, remote node).
 *
 * Each dispatched machine file (e.g. dumper_truck.h) uses an IS_MACHINE
 * compile-time guard to expose only its enum section when IS_MACHINE is absent.
 *
 * NOTE: when adding a new machine type, add one #elif branch below AND the
 * matching branch in combus.h.
 *******************************************************************************/// 
#pragma once

// Each machine file now owns its enum definitions directly (section 1, always compiled).
// combus_types.h dispatches to the same machine files as combus.h — the IS_MACHINE
// guard inside each file ensures that cross-boundary consumers (sound node, etc.)
// receive only the enum IDs, without structs or input-mapping headers.
#if defined(MACHINE_TYPE) && (MACHINE_TYPE == DUMPER_TRUCK)
  #include "dumper_truck/dumper_truck.h"

#elif defined(MACHINE_TYPE) && (MACHINE_TYPE == AUTRE_MACHINE_EXEMPLE)
  #include "autre_machine/autre_machine.h"

#else
    #error "No machine type defined for com-bus. Check platformio.ini file and env:xxx setting to fix the problem"
#endif


// EOF combus_types.h