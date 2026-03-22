/*!****************************************************************************
 * @file  combus_struct.h
 * @brief Internal communication bus structure definition
 * This structure is used to standardized communication into nodes. It act as a harware
 * abstraction layer between different input modules (RC protocol, bluetooth, wifi ...), the main code 
 * and output modules (IO expander, sound module). Its data structure is devided in channels :
 * - Digital channel to store two state date
 * - Analog channel to store analog/multi state data
 * - A runlevel data to store machine state
 * 
 * 
 * NOTE:
 * - do not change uint16_t size for AnalogComBus. Some system sub value depend of this size
 * - "isDrived" flag have to be set true if its channel is perodicaly update.
 *   For safety, a watchdog should manage a disconnect timout an set "isDrived" false after a delay.
 * - All input/output modules had to write/read this struct to share data
 *******************************************************************************/// 
#pragma once

#include <stdint.h>

#include <defs/machines_defs.h>


// =============================================================================
// COMBUS CHANNEL OWNERSHIP
// =============================================================================

/**
 * @brief Which module is allowed to write this ComBus channel.
 *
 * @details Each channel stores one `ChanOwner` value that names the module
 *   responsible for writing it. All other modules must only read it.
 *
 *   At init time a module checks `owner` to decide if it should write the
 *   channel or just read it. The same channel can have a different owner
 *   depending on the build environment. Example: `runLevel` is owned by
 *   `MACHINE` when a machine node is present; it is owned by `SOUND` when
 *   the sound module runs standalone with no machine master.
 *
 *   `NONE` — no owner declared, channel is passive (safe default).
 *   `ANY`  — any module may write; use only for shared scratch channels.
 */
enum class ChanOwner : uint8_t {
    NONE     = 0,   ///< No writer declared — channel is passive / read-only
    ANY,            ///< Any module may write (shared scratchpad — use sparingly)
    MACHINE,        ///< Written by the machine main node (RunLevel FSM, motion control)
    INPUT,          ///< Written by the input module (PS4, SBUS, Wi-Fi remote…)
    SOUND,          ///< Written by the sound module (standalone mode only)
    VBAT,           ///< Written by the battery monitor module
    REMOTE,         ///< Written by a remote node over a communication link
};


// =============================================================================
// CHANNEL STRUCTS
// =============================================================================

  // analogic bus data structure
 typedef struct {
  const char* infoName;                    // analog bus short description
  uint16_t value;                          // analog bus current value
  bool isDrived   = false;                 // true if the AnalogComBus have a driving source
  ChanOwner owner = ChanOwner::NONE;  // authoritative writer — see ChanOwner
} AnalogComBus;

  // digital bus data structure (two state)
 typedef struct {
  const char* infoName;                     // digital bus short description
  bool value;                               // digital bus current value (true of false)
  bool isDrived   = false;                  // true if the DigitalComBus have a driving source
  ChanOwner owner = ChanOwner::NONE;   // authoritative writer — see ChanOwner
} DigitalComBus;

  // Main communication bus structure
typedef struct {
  RunLevel runLevel;                            // runlevel state
  bool batteryIsLow    = false;                 // true when any vbat channel reports low — written by vbat module, read by all
  bool keyOn           = false;                 // operator ignition consent — derived from input, used by state machine transitions
  uint32_t lastFrameMs = 0;                     // millis() of the last successful combus_frame_apply — used by watchdog
  AnalogComBus* analogBus;                      // analogic bus channels
  DigitalComBus* digitalBus;                    // digital bus channels
  uint32_t analogBusMaxVal;                     // Com-bus analog channel maximum value
} ComBus;

// EOF combus_struct.h