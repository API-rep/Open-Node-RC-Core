/*!****************************************************************************
 * @file  combus_types.h
 * @brief Com-bus enum types top file
 * This file is the umbrella for com-bus enum types definition. From -DMACHINE flag, it
 * includes the correct enum types definition file into the project.
 * NOTE:
 * - This file is paired with combus.h file which define the com-bus structure.
 *   Splitting types and structure definition avoid include loop issues.
 *   So, don't forget to update both files when a new machine/combus type is added
 *   to the project. Une -D MACHINE compilation flag as test argument for each file.
 *******************************************************************************/// 
#pragma once

/**
 * @brief Com-bus enum types inclusion file selec
 */

#if defined(MACHINE_TYPE) && (MACHINE_TYPE == DUMPER_TRUCK)
  #include "dumper_truck/dumper_truck_types.h"

#elif defined(MACHINE_TYPE) && (MACHINE_TYPE == AUTRE_MACHINE_EXEMPLE)
  #include "autre_machine/autre_machine_types.h"

#else
    #error "No machine type defined for com-bus. Check platformio.ini file and env:xxx setting to fix the problem"
#endif


// EOF combus_types.h