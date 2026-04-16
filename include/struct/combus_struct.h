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
 * @brief Raw bitmask constants for NodeGroup/ProcessOwner ChanOwner values.
 *
 * @details One byte = two nibbles:
 *   - bits 7..4  NodeGroup   (GRP_*):  physical node that natively own the channel.
 *   - bits 3..0  ProcessRole (PROC_*): process on that node that is authorised to write.
 *
 *   Composed ChanOwner enum values (e.g. GRP_MACHINE | PROC_INPUT) are built
 *   from these constants.
 *
 *   Sentinels (fixed by contract — do not renumber):
 *   - GRP_NONE  / PROC_NONE  (0x0) — absent / unassigned.
 *   - GRP_ANY   / PROC_ANY   (0xF) — wildcard (see _owner_ok logic).
 *
 *   Slots 0x5..0xE are free for future groups and roles.
 */
namespace ComBusOwner {

    // --- NodeGroup nibble (bits 7..4) ---
    static constexpr uint8_t GRP_MASK    = 0xF0u;   ///< isolates the group nibble
    static constexpr uint8_t GRP_NONE    = 0x00u;   ///< 0000 — no group declared
    static constexpr uint8_t GRP_MACHINE = 0x10u;   ///< 0001 — primary controller node
    static constexpr uint8_t GRP_SOUND   = 0x20u;   ///< 0010 — sound node
    static constexpr uint8_t GRP_REMOTE  = 0x30u;   ///< 0011 — remote controller
    static constexpr uint8_t GRP_EXT     = 0x40u;   ///< 0100 — extension board
    //                                     ...       ///< 0101..1110 — reserved
    static constexpr uint8_t GRP_ANY     = 0xF0u;   ///< 1111 — wildcard: process match only

    // --- ProcessRole nibble (bits 3..0) ---
    static constexpr uint8_t PROC_MASK   = 0x0Fu;   ///< isolates the process role nibble
    static constexpr uint8_t PROC_NONE   = 0x00u;   ///< 0000 — no process declared
    static constexpr uint8_t PROC_SYSTEM = 0x01u;   ///< 0001 — FSM / state machine / local system
    static constexpr uint8_t PROC_INPUT  = 0x02u;   ///< 0010 — input layer (PS4, SBUS, …)
    static constexpr uint8_t PROC_VBAT   = 0x03u;   ///< 0011 — battery monitor
    static constexpr uint8_t PROC_BRIDGE = 0x04u;   ///< 0100 — UART RX bridge (combus_frame_apply)
    //                                     ...       ///< 0101..1110 — reserved
    static constexpr uint8_t PROC_ANY    = 0x0Fu;   ///< 1111 — wildcard (only valid in NONE/ANY slots)

} // namespace ComBusOwner


/**
 * @brief ComBus channel access identity — one byte, two nibbles.
 *
 * @details Each named value is composed from a ComBusOwner::GRP_* constant (high nibble)
 *   and a ComBusOwner::PROC_* constant (low nibble).  Changing a raw constant in ComBusOwner
 *   propagates automatically to every named owner — no cascading edits needed.
 *
 *   Access rules applied by combus_access (_owner_ok):
 *   - NONE (0x00)              — channel unclaimed; write denied for all callers.
 *   - ANY  (0xFF)              — unrestricted; any caller may write.
 *   - GRP == GRP_ANY, PRC != PROC_ANY — wildcard group: any node's matching process.
 *   - GRP == this node's group — local domain: full byte match required.
 *   - GRP != this node's group — foreign domain: PROC_BRIDGE only (UART RX bridge).
 *
 *   Helper inlines: chanOwnerGroup() / chanOwnerProcess() (see below).
 */
enum class ChanOwner : uint8_t {
    NONE           = ComBusOwner::GRP_NONE    | ComBusOwner::PROC_NONE,    ///< 0x00 — unclaimed

    // Machine node (GRP_MACHINE = 0x10)
    MACHINE_SYSTEM = ComBusOwner::GRP_MACHINE | ComBusOwner::PROC_SYSTEM,  ///< 0x11 — machine FSM / RunLevel / key-on
    MACHINE_INPUT  = ComBusOwner::GRP_MACHINE | ComBusOwner::PROC_INPUT,   ///< 0x12 — machine input layer
    MACHINE_VBAT   = ComBusOwner::GRP_MACHINE | ComBusOwner::PROC_VBAT,   ///< 0x13 — machine battery monitor
    MACHINE_BRIDGE = ComBusOwner::GRP_MACHINE | ComBusOwner::PROC_BRIDGE,  ///< 0x14 — machine UART RX bridge

    // Sound node (GRP_SOUND = 0x20)
    SOUND_SYSTEM   = ComBusOwner::GRP_SOUND   | ComBusOwner::PROC_SYSTEM,  ///< 0x21 — sound node FSM
    SOUND_BRIDGE   = ComBusOwner::GRP_SOUND   | ComBusOwner::PROC_BRIDGE,  ///< 0x24 — sound UART RX bridge

    // Remote controller (GRP_REMOTE = 0x30)
    REMOTE_SYSTEM  = ComBusOwner::GRP_REMOTE  | ComBusOwner::PROC_SYSTEM,  ///< 0x31 — remote FSM
    REMOTE_INPUT   = ComBusOwner::GRP_REMOTE  | ComBusOwner::PROC_INPUT,   ///< 0x32 — remote input layer

    // Extension board (GRP_EXT = 0x40)
    EXT_SYSTEM     = ComBusOwner::GRP_EXT     | ComBusOwner::PROC_SYSTEM,  ///< 0x41 — extension board FSM

    // Wildcard-group owners (GRP_ANY = 0xF0): any node's matching process may write
    VBAT_MON       = ComBusOwner::GRP_ANY     | ComBusOwner::PROC_VBAT,    ///< 0xF3 — any node's battery monitor

    // Unrestricted
    ANY            = ComBusOwner::GRP_ANY     | ComBusOwner::PROC_ANY,     ///< 0xFF — anyone may write
};

/** Returns the group nibble (bits 7..4) of a ChanOwner value. */
constexpr uint8_t chanOwnerGroup  (ChanOwner o) { return static_cast<uint8_t>(o) & ComBusOwner::GRP_MASK;  }

/** Returns the process role nibble (bits 3..0) of a ChanOwner value. */
constexpr uint8_t chanOwnerProcess(ChanOwner o) { return static_cast<uint8_t>(o) & ComBusOwner::PROC_MASK; }


/**
 * @brief Compose a `ChanOwner` from a node group and a process role.
 *
 * @details Use in env-agnostic code instead of hardcoded named owners:
 *   @code
 *     combus_set_runlevel(bus, lvl, makeChanOwner(COMBUS_ENV_NODE_GROUP, ComBusOwner::PROC_SYSTEM));
 *     combus_set_battlow (bus, v,   makeChanOwner(COMBUS_ENV_NODE_GROUP, ComBusOwner::PROC_VBAT));
 *     combus_frame_apply (cfg, bus, snap, makeChanOwner(COMBUS_ENV_NODE_GROUP, ComBusOwner::PROC_BRIDGE));
 *   @endcode
 *
 * @param grp   A `ComBusOwner::GRP_*` constant — pass `COMBUS_ENV_NODE_GROUP` from env config.
 * @param proc  A `ComBusOwner::PROC_*` constant.
 * @return      Composed `ChanOwner` value.
 */
constexpr ChanOwner makeChanOwner(uint8_t grp, uint8_t proc) {
    return static_cast<ChanOwner>(grp | proc);
}


// =============================================================================
// CHANNEL STRUCTS
// =============================================================================

  // analogic bus data structure
 typedef struct {
  const char* infoName;                    // analog bus short description
  uint16_t value;                          // analog bus current value
  bool isDrived   = false;                 // true if the AnalogComBus have a driving source
  ChanOwner owner = ChanOwner::NONE;       // authoritative writer — see ChanOwner
} AnalogComBus;

  // digital bus data structure (two state)
 typedef struct {
  const char* infoName;                     // digital bus short description
  bool value;                               // digital bus current value (true of false)
  bool isDrived   = false;                  // true if the DigitalComBus have a driving source
  ChanOwner owner = ChanOwner::NONE;        // authoritative writer — see ChanOwner
} DigitalComBus;

  // Main communication bus structure
typedef struct {
  RunLevel  runLevel;                           // machine run level — written by the MACHINE module FSM
  ChanOwner runLevelOwner = ChanOwner::NONE;    // module allowed to write runLevel — set at init
  bool batteryIsLow = false;                    // true when any vbat channel reports low — written by vbat module, read by all
  ChanOwner battLowOwner = ChanOwner::NONE;     // module allowed to write batteryIsLow — set at init
  bool keyOn = false;                           // operator ignition consent — derived from input, used by state machine transitions
  ChanOwner keyOnOwner = ChanOwner::NONE;       // module allowed to write keyOn — set at init
  uint32_t lastFrameMs = 0;                     // millis() of the last successful combus_frame_apply — used by watchdog
  AnalogComBus* analogBus;                      // analogic bus channels
  DigitalComBus* digitalBus;                    // digital bus channels
  uint32_t analogBusMaxVal;                     // Com-bus analog channel maximum value
} ComBus;

// EOF combus_struct.h