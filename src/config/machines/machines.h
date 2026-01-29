/*!****************************************************************************
 * @file machines.h
 * @author API
 * @brief Top machines configuration file.
 * This file contain all available machines configuation files supported by the project.
 * To use ONE of them, uncomment the machine line.
 * Multiple uncommented line will cause unatended compilation result.
 *******************************************************************************/// 
#pragma once

#include "const.h"
#include "struct.h"
#include "macro.h"

  /* TP dumper trucks */
#if MACHINE == VOLVO_A60_H_BRUDER    // Bruder Volvo_A60H full electric conversion
  #include "volvo_A60H_bruder.h"
#endif

#ifndef MACHINE 
 #error "No machine defined for compilation. Check config.h file or -DMACHINE=... command line parameter to fix the problem" 
#endif


#include "boards/boards.h"

// EOF boards.h