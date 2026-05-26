/*!****************************************************************************
 * @file    volvo_A60H_bruder.h
 * @brief   Volvo A60H Bruder — vehicle configuration.
 *
 * @details Provides vehicle-level declarations for all execution environments:
 *   - `kVehicleName`, `kVehicleCombusLayout` — identity constants (always present).
 *   - `SigDev` enum and `sigDevArray` extern — IS_MAINBOARD only.
 *   Dispatches to the board-specific environment based on the build flag:
 *   - `-D IS_MAINBOARD`  → machine main board (dispatches on -D BOARD).
 *   - `-D IS_EXT_BOARD`  → machine extension board (dispatches on -D BOARD).
 *
 *   Array definitions live in `volvo_A60H_bruder.cpp`.
 *
 *   Architecture:
 *   @code
 *   machines.h
 *     └── volvo_A60H_bruder/volvo_A60H_bruder.h   ← this file
 *           ├── [kVehicleName, kVehicleCombusLayout]   (shared)
 *           ├── [SigDev enum, sigDevArray]               (IS_MAINBOARD only)
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
// IS_MAINBOARD — vehicle-level device tables + board dispatch
// =============================================================================

#if defined(IS_MAINBOARD)

#include <struct/struct.h>
#include <defs/defs.h>
#include <core/config/machines/combus_ids.h>

/**
 * @brief Indices into `sigDevArray[]`.
 *
 * @details Signal devices carry a ComBus channel reference and a `DevUsage`
 *   tag.  Entries tagged `UNDEFINED` are handled by dedicated module logic
 *   (engine key FSM, indicator/hazard mux in sound_core).
 */
enum SigDev {
    HORN_SIG    = 0,   ///< Horn trigger → DevUsage::SIG_HORN.
    LIGHTS_SIG,        ///< Main lights toggle → DevUsage::SIG_LIGHT.
    KEY_SIG,           ///< Engine on/off key → DevUsage::UNDEFINED (FSM).
    INDIC_L_SIG,       ///< Left indicator → DevUsage::UNDEFINED (mux).
    INDIC_R_SIG,       ///< Right indicator → DevUsage::UNDEFINED (mux).
    HAZARDS_SIG,       ///< Hazard flashers → DevUsage::UNDEFINED (mux).
    SIG_COUNT          ///< Sentinel — number of signal devices.
};

/// Array definitions in `volvo_A60H_bruder.cpp`.
extern SigDevice sigDevArray[SIG_COUNT];

#include <core/config/machines/dumper_truck/combus/processors/input_proc_config.h>  ///< kInputChains[], InputCh enum — dumper-truck class.
#include <core/config/machines/dumper_truck/combus/processors/sim_proc_config.h>    ///< kSimChannels[], SimCh enum — dumper-truck class.

// --- Board dispatch ---
  #include "mainboard/mainboard.h"

#elif defined(IS_EXT_BOARD)
  #include "ext_board/ext_board.h"
#endif
  // No #else error here: sound_node and remote envs include this file for
  // kVehicleName / kVehicleCombusLayout only — they define neither flag.

// EOF volvo_A60H_bruder.h
