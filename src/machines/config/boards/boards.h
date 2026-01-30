/*!****************************************************************************
 * @file boards.h
 * @brief Top boards configuration file.
 * This file contain all available boards configuation files supported by the project.
 * To use ONE of them, uncomment the board line.
 * Multiple uncommented line will cause unatended compilation result.
 *******************************************************************************/// 
#pragma once

  /* ESP32 based board */
#if BOARD == ESP32_8M_6S    // Bruder Volvo_A60H full electric conversion
  #include "ESP32_8M_6S.h"
#endif

#ifndef BOARD 
 #error "No motherboard defined this vehicle. Check machine/machine_name.h file to fix the problem"
#endif

// EOF boards.h