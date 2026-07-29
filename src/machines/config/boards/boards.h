/*!****************************************************************************
 * @file boards.h
 * @brief Top boards configuration file.
 * This file contain all available boards configuation files supported by the project.
 * To use ONE of them, uncomment the board line.
 * Multiple uncommented line will cause unatended compilation result.
 *******************************************************************************/// 
#pragma once

  /* ESP32 based board */
#if defined(BOARD_ESP32_8M_6S)
  #include "ESP32_8M_6S.h"

// #elif defined(BOARD_ANOTHER_BOARD)
//   #include "another_board.h"

#else
 #error "No motherboard defined this vehicle. Check BOARD_* flag to fix the problem"
#endif

// EOF boards.h