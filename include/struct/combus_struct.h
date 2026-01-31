/*!****************************************************************************
 * @file  combus_struct.h
 * @brief Internal communication bus structure definition
 * This structure is used to standardized communication into main code. It act as a harware
 * abstraction layer between different input modules (RC protocol, bluetooth, wifi ...), the main code 
 * and output modules (IO expander, sound module). Its data structure is devided in channels :
 * - Digital channel to store two state date
 * - Analog channel to store analog/multi state data
 * - A runlevel data to store machine state
 * 
 * 
 * NOTE:
 * - do not change uint16_t size for AnalogBus. Some system sub value depend of this size
 * - "isDrived" flag have to be set true if its channel is perodicaly update.
 *   For safety, a watchdog should manage a disconnect timout an set "isDrived" false after a delay.
 * - All input/output modules had to write/read this struct to share data
 *******************************************************************************/// 
#pragma once

#include <stdint.h>

#include <const.h>
#include <defs/defs.h>


  // analogic bus data structure
 typedef struct {
  const char* infoName;                         // analog bus short description
  uint16_t value;                               // analog bus current value
  bool isDrived = false;                        // true if the AnalogBus have a driving source
} AnalogBus;

  // digital bus data structure (two state)
 typedef struct {
  const char* infoName;                         // digital bus short description
  bool value;                                   // digital bus current value (true of false)
  bool isDrived = false;                        // true if the DigitalBus have a driving source
} DigitalBus;

  // Main communication bus structure
typedef struct {
  RunLevel runLevel;                            // runlevel state
  AnalogBus* analogBus;                         // analogic bus channels
  DigitalBus* digitalBus;                       // digital bus channels
  uint32_t analogBusMaxVal;                     // Com-bus analog channel maximum value
} ComBus;

// EOF combus_struct.h