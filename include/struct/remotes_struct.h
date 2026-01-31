/*!****************************************************************************
 * @file  remotes_struct.h
 * @brief Remote control structure definition file
 * This file contain the structure used to define remote control device configuration
 * and features.
 *******************************************************************************/// 
#pragma once

#include <stdint.h>

#include <const.h>
#include <defs/defs.h>


/**
 * @brief Internal input device data structure
 * This structure is used to store input device data.It act as a harware abstraction layer between 
 * devices input module (RC protocol, bluetooth, wifi ...) and internal communication bus(com-bus).
 * Its data structure is devided in channels :
 * - Digital channel to store two state devices (button, switches ...)
 * - Analog channel to store analog devices (sticks, slider, trimmers ...)
 * - A status flag use to track input device state (disconnect timout i.e.)
 * - An analog/digital channels counter
 * 
 * Its instance is create in init\input_init.h file
 * For convenance, a user editable liker map input device data to com-bus channel. By this, it allow
 * 
 *
 * 
 * NOTE:
 * - do not change uint16_t size for devices. Some system sub value depend of this size
 * - "status" flag have to be set true if input device data perodicaly update.
 *   For safety, a input module watchdog should manage a disconnect timout an set "status" to disconnect after a delay.
 * - All input device modules had to write this struct to share incomming data
 */

  // analog devices (sticks, analog buttons, sliders)
typedef struct {
  const char* infoName;                       // device short description
  RemoteComp type = RemoteComp::UNDEFINED;    // analog device type
  uint16_t val;                               // analog device value
  const int32_t minVal = 0;                   // minimum value return by analog device decoder module at lower state
  const int32_t maxVal = 0;                   // maximum value return by analog device decoder module at higher state
  const bool isInverted = false;              // true if analog device axe is inverted
} AnalogDev;

  // digital devices (pushbuttons, switches)
typedef struct {
  const char* infoName;                       // device short description
  RemoteComp type = RemoteComp::UNDEFINED;    // switch device type
  bool val;                                   // digital device value
  const bool isInverted = false;              // true if switch logic is inverted
} DigitalDev;

  // remote data structure
typedef struct {
  const char* infoName;                                 // remote short description
  RemoteProtocol protocol = RemoteProtocol::UNDEFINED;  // Remote protocol definition{
  AnalogDev* analogDev;                                 // pointer to external AnalogDev structure
  DigitalDev* digitalDev;                               // pointer to external DigitalgDev structure
  uint8_t analogDevCount;                               // number of input analog channel
  uint8_t digitalDevCount;                              // number of input digital channel
  Status status = Status::NOT_SET;                      // remote controle status
} Remote;

// EOF remotes_struct.h