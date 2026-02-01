/*!****************************************************************************
 * @file  remotes_map_struct.h
 * @brief Remote/input devices to com-bus mappingstructure definition file
 * This file contain the structure used to map remote control input devices to
 * internal communication bus (com-bus) channels. 
 * ONLY include this file AFTER com-bus structure definition file, in init scripts.
 *******************************************************************************/// 
#pragma once

#include <stdint.h>
#include <defs/defs.h>

  // enum class forward declaration
  // will be declared next in map config files
enum class AnalogInputDevID : uint8_t;
enum class AnalogComBusID : uint8_t;
enum class DigitalInputDevID : uint8_t;
enum class DigitalComBusID : uint8_t;

typedef struct {
  AnalogInputDevID devID;    // L'index dans AnalogInputDevArray (ex: LY_STICK)
  AnalogComBusID busChannel;  // Le canal cible (ex: DRIVE_SPEED_BUS)
  bool isInverted;            // Logique d'inversion
} InputAnalogMap;

typedef struct {
  DigitalInputDevID devID;   // L'index dans digitalInputDevArray (ex: CROSS_BTN)
  DigitalComBusID busChannel; // Le canal cible (ex: HORN)
  bool isInverted;            // Logique d'inversion
} InputDigitalMap;


// EOF remotes_map_struct.h