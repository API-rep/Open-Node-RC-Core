/*!****************************************************************************
 * @file  core_defs.h
 * @brief RC system core environement definitions.
 * @details This file defines datas common on all core nodes.
 *******************************************************************************/// 
#pragma once

#include <cstdint>


// =============================================================================
// COMBUS LAYOUT ENUM
// =============================================================================

/**
 * @brief Com-bus layout selector — identifies the ComBus frame structure
 *   (analog/digital channel count and roles) of a vehicle.
 *
 * @details Used as `kVehicleCombusLayout` in vehicle entry-point headers.
 *   In each vehicle header, `#define MACHINE_TYPE <member>` (e.g.
 *   `#define MACHINE_TYPE DUMPER_TRUCK`) exposes the enum value for C++ use
 *   via `CombusLayout::MACHINE_TYPE`.
 *   Preprocessor dispatch (`#if MACHINE == VOLVO_A60_H_BRUDER`) uses the
 *   -D MACHINE= build flag directly — no separate integer constants needed.
 */
enum class CombusLayout : uint8_t {
  UNDEFINED     =  0,   ///< No layout assigned
  DUMPER_TRUCK  =  1,   ///< Articulated hauler / dump truck
  EXCAVATOR     =  2,   ///< Hydraulic-arm excavator
  WHEEL_LOADER  =  3,   ///< Wheel loader (bucket)
};


// EOF core_defs.h