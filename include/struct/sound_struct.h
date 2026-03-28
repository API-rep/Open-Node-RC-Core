/*!****************************************************************************
 * @file  sound_struct.h
 * @brief Sound module channel descriptor.
 *
 * @details Defines `SoundChannel`, the static descriptor stored in the
 *   vehicle sound profile tables (`kSoundProfile[]`).  Each entry binds one
 *   ComBus channel index to an `ApplyFn` dispatch function; the profile table
 *   is declared per vehicle in
 *   `sound_module/config/profiles/<vehicle>/dumper_truck.h` and lives const
 *   in Flash.
 *******************************************************************************
 */
#pragma once

#include <stdint.h>
#include <defs/machines_defs.h>


// Forward declaration — full type in callers via combus_frame.h.
struct ComBusFrame;


// =============================================================================
// 1. SOUND CHANNEL
// =============================================================================

/**
 * @brief Dispatch function: extract one channel value and forward to SoundCore.
 *
 * @details Called once per ComBus update cycle for each `SoundChannel` entry.
 *   @p snap is the current valid frame; @p chanID is the channel index stored
 *   in the owning `SoundChannel`.  The function reads `snap->analog[chanID]`
 *   or `snap->digital[chanID]` as appropriate and calls the corresponding
 *   `sound_core_set_*()` entry point.  Compound functions (e.g. indicators)
 *   may ignore `chanID` and read multiple channels directly from @p snap.
 */
using ApplyFn = void (*)(const ComBusFrame*, uint8_t);


/**
 * @brief Static descriptor for one ComBus channel → sound role binding.
 *
 * @details Entries are produced at compile time in the vehicle profile table
 *   (`kSoundProfile[]`).  The `usage` field carries the peripheral role for
 *   diagnostics and dashboard display; the interpreter calls
 *   `apply(snap, chanID)` without inspecting `usage`.
 *
 *   `isDigital` records the ComBus bus type and is intended for tooling and
 *   dashboard display.  The correct bus type is already encoded in `apply`.
 */
struct SoundChannel {
	DevUsage  usage;      ///< Peripheral role — for diagnostics and dashboard.
	uint8_t   chanID;     ///< ComBus channel index forwarded to apply().
	bool      isDigital;  ///< true = DigitalComBus channel, false = AnalogComBus.
	ApplyFn   apply;      ///< Dispatch fn — reads snap, calls sound_core_set_*().
};

// EOF sound_struct.h
