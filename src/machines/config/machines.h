/*!****************************************************************************
 * @file machines.h
 * @brief Top machines configuration file.
 * This file contain all available machines configuation files supported by the project.
 * To use ONE of them, uncomment the machine line in main config file or specify
 * a -DMACHINE=... parameter in compiler command line. 
 *******************************************************************************/// 
#pragma once

  /* TP dumper trucks */
#if MACHINE == VOLVO_A60_H_BRUDER    // Bruder Volvo_A60H full electric conversion
  #include "volvo_A60H_bruder.h"
#endif

#ifndef MACHINE 
 #error "No machine defined for compilation. Check config.h file or -DMACHINE=... command line parameter to fix the problem" 
#endif


#include "boards/boards.h"

// EOF boards.h