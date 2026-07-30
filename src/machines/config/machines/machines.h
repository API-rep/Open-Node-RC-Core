/*!****************************************************************************
 * @file machines.h
 * @brief Top machines dispatcher file.
 * This dispatcher is the single entry point for machines configuation files.
 * To use ONE of them, specify a -D MACHINE_* parameter in compiler command line
 * or platformio.ini file. 
 *******************************************************************************/// 
#pragma once

/* TP dumper trucks */
#if defined (MACHINE_VOLVO_A60_H_BRUDER)
  #include "volvo_A60H_bruder/volvo_A60H_bruder.h"

// #elif defined (MACHINE_ANOTHER_MACHINE)
//   #include "another_machine.h"

#elif defined (MACHINE_NONE)
  // include nothing. No machine selected

#else
  #error "Unsupported/missing MACHINE_* value. Check platformio.ini file to fix the problem"
#endif

// EOF machines.h
