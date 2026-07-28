/*!****************************************************************************
 * @file machines.h
 * @brief Top machines configuration file.
 * This file contain all available machines configuation files supported by the project.
 * To use ONE of them, uncomment the machine line in main config file or specify
 * a -DMACHINE=... parameter in compiler command line. 
 *******************************************************************************/// 
#pragma once

#ifndef MACHINE
  #error "No machine defined for compilation. Check platformio.ini file and env:xxx MACHINE setting to fix the problem"
#endif

/* TP dumper trucks */
#if MACHINE == VOLVO_A60_H_BRUDER
  #include "volvo_A60H_bruder/volvo_A60H_bruder.h"

// #elif MACHINE == ANOTHER_MACHINE
//   #include "another_machine.h"

#else
  #error "Unsupported MACHINE value. Check platformio.ini file and env:xxx MACHINE setting to fix the problem"
#endif

// EOF machines.h
