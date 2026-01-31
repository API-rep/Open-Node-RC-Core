/*!****************************************************************************
 * @file input.h
 * @brief Global input module configuration file
 * This section contain all input module sub include files. 
 * 
 * This script MUST be include in the top of main.cpp or config.h
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>

#include <config/config.h>

  //Devices decoded directly by the onboard controller
#if INPUT_MODULE == PS4_DS4_BT    // PS4 Dualshock 4 controller 
  #include "PS4_dualshock4.h"
#endif

#ifndef INPUT_MODULE
 #error "No input module defined. Check config files or -D INPUT_MODULE=... build flag to fix the problem" 
#endif

// EOF input.h