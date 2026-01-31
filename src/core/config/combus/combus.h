/*!****************************************************************************
 * @file  combus_types.h
 * @brief Com-bus enum types top file
 * This file include all available com-bus enums definition.
 * It allows to define diferent com-bus configuration and to include
 * the correct one in input device to com-bus mapping structure
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>


  /* TP dumper trucks */
#if MACHINE_TYPE == DUMPER_TRUCK
  #include "dumper_truck.h"
#endif

#ifndef MACHINE_TYPE 
 #error "No machine type defined for com-bus. Check platformio.ini file and env:xxx setting to fix the problem" 
#endif


// EOF combus.h