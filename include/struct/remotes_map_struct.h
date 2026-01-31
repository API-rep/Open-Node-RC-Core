/*!****************************************************************************
 * @file  remotes_map_struct.h
 * @brief Remote/input devices to com-bus mappingstructure definition file
 * This file contain the structure used to map remote control input devices to
 * internal communication bus (com-bus) channels. 
 * ONLY include this file AFTER com-bus structure definition file
 *******************************************************************************/// 
#pragma once

#include <stdint.h>
#include <defs/defs.h>


typedef struct {
  RemoteComp component;      // analogic remote componnent type (ex: LStickX, RStickY)
  AnalogComCh busChannel;    // analogic com-bus channel (ex: STEERING_BUS)
  bool isInverted = false;   // control inversion logic
} RemoteAnalogMapping;

typedef struct {
  RemoteComp component;      // digital remote componnent type (ex: HORN, LIGHTS)
  DigitComCh busChannel;     // digital com-bus channel (ex: BRAKE, HORN)
  bool isInverted = false;   // control inversion logic
} RemoteDigitalMapping;


// EOF remotes_map_struct.h