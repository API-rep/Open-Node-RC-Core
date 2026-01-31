/*!****************************************************************************
 * @file  combus.h
 * @brief Com-bus top configuration file
 * This file include all available com-bus configuration.
 * Com-bus is used for internal communication between nodes (remote, machine ...) 
 * and is the base of the RC core environement. Due to the wide variety of possible
 * configuration, multiple com-bus templates are available.
 * 
 * Com-bus must contain all control channels used by the machine such as :
 * - Analog channels (steering, speed, actuator position ...)
 * - Digital channels (lights, horn, ...)
 * - Machine state (runlevel, ...)
 * NOTE:
 * - Cummunication chain :
 *   1. Remote side
 *     - Vehicle config definition :
 *      -> Vehicle ID, address, combus template (combus.h), name ...
 *     - Map remote devices to combus channels (analog, digital ...)
 *     - Send combus packet via output module (BT, WiFi, RF ...)
 *       .
 *       . 
 *       .
 *   2. Machine side
 *    - Receive combus packet via input module (BT, WiFi, RF ...)
 *    - Parse combus packet and fill machine combus structure (combus.h)
 *    - Machine side com-bus exploitation (see machine sub project for more info)
 * 
 *******************************************************************************/// 
#pragma once

#include <const.h>
#include <struct/struct.h>
#include <defs/defs.h>


  /* TP dumper trucks */
#if MACHINE_TYPE == DUMPER_TRUCK
  #include "combus/dumper_truck.h"
#endif

#ifndef MACHINE_TYPE 
 #error "No machine type defined for com-bus. Check platformio.ini file and env:xxx setting to fix the problem" 
#endif


// EOF combus.h