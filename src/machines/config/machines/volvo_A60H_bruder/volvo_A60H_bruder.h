/*!****************************************************************************
 * @file    volvo_A60H_bruder.h
 * @brief   Volvo A60H Bruder — vehicle entry point.
 *
 * @details Declares vehicle-level identity constants shared between all
 *   execution environments (machine node, sound node, future remote…),
 *   then dispatches to the correct environment-type umbrella based on the
 *   build flag:
 *   - `-D IS_MAINBOARD`  → machine main board (dispatches on -D BOARD).
 *   - `-D IS_EXT_BOARD`  → machine extension board (dispatches on -D BOARD).
 *
 *   Architecture:
 *   @code
 *   machines.h
 *     └── volvo_A60H_bruder/volvo_A60H_bruder.h   ← this file
 *           ├── [kVehicleName, kVehicleCombusLayout]   (Level 0 — shared)
 *           ├── mainboard/mainboard.h              (IS_MAINBOARD → BOARD dispatch)
 *           └── ext_board/ext_board.h              (IS_EXT_BOARD → BOARD dispatch)
 *   @endcode
 *******************************************************************************
 */
#pragma once

#include <defs/core_defs.h>   // CombusLayout

/// Machine class selector — expands to a CombusLayout member token so that
/// `CombusLayout::MACHINE_TYPE` resolves correctly in sound_module/config/config.h.
#define MACHINE_TYPE  DUMPER_TRUCK


// =============================================================================
// Level-0 — vehicle identity (shared between all environments)
// =============================================================================

/// Vehicle display name — used by EnvCfg.infoName in all environment configs.
inline constexpr const char*  kVehicleName         = "Volvo A60H Bruder";

/// ComBus frame layout — shared between machine and sound nodes.
inline constexpr CombusLayout kVehicleCombusLayout  = CombusLayout::DUMPER_TRUCK;


// =============================================================================
// Environment-type dispatch
// =============================================================================

#if defined(IS_MAINBOARD)
  #include "mainboard/mainboard.h"
#elif defined(IS_EXT_BOARD)
  #include "ext_board/ext_board.h"
#endif
  // No #else error here: sound_node and remote envs include this file for
  // kVehicleName / kVehicleCombusLayout only — they define neither flag.

// EOF volvo_A60H_bruder.h
