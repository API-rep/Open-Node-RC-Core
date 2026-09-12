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
// FORWARD DECLARATIONS — OPAQUE ENUM TYPES
// =============================================================================

/// @brief Forward declarations only — AnalogComBusID/DigitalComBusID
///   as opaque enum types (fixed underlying type = complete type in
///   C++11+, sufficient for use as field/parameter types without
///   the full definition).
///
/// Global scope: only one machine config is visible per translation
/// unit today. If a multi-machine remote build ever needs several
/// machine namespaces in the same TU, reintroduce namespacing then,
/// alongside the .inc selection logic — not needed now.
///
/// For the FULL definition (enumerators + runtime comBus/arrays),
/// include the machine's own combus.h / combus_ids.h instead.

enum class AnalogComBusID  : uint8_t;
enum class DigitalComBusID : uint8_t;


// =============================================================================
// COMBUS CHANNEL LAYERS
// =============================================================================

/**
 * @brief ComBus channel propagation layer.
 *
 * @details Defines the visibility scope of each channel:
 *   - UNDEFINED : Layer not yet assigned (default for uninitialized channels)
 *   - SYSTEM    : Private to a single board's firmware (intra-device)
 *   - LOCAL     : Shared between all boards of the same system (intra-node)
 *   - REMOTE    : Exchanged between distinct nodes (inter-node)
 *
 * Access rules:
 *   1. SYSTEM  → Only the local firmware may write
 *   2. LOCAL   → Local firmware + extension boards (same system)
 *   3. REMOTE  → Any source (local + extensions + remote nodes)
 *
 * Layer inheritance:
 *   - LOCAL channels automatically include all REMOTE channels
 *   - SYSTEM data can be promoted to LOCAL if needed
 *   - REMOTE represents the minimal universal service set
 */
enum class ChanLayer : uint8_t {
    UNDEFINED = 0x00,   ///< Layer not yet assigned (uninitialized)
    SYSTEM    = 0x01,   ///< Intra-device: private to a single board's firmware
    LOCAL     = 0x02,   ///< Intra-node: shared between all boards of the same system
    REMOTE    = 0x03    ///< Inter-node: exchanged between distinct nodes
};


// =============================================================================
// CHANNEL DIRECTION (bitmask — added by A6.1)
// =============================================================================

/**
 * @brief Wire direction of a channel (bitmask, 1 byte).
 *
 * @details bit 0 = uplink, bit 1 = downlink.
 *   None     = 0
 *   Uplink   = 1
 *   Downlink = 2
 *   Both     = 3 (= Uplink | Downlink)
 *
 * @note A6.1: added as a runtime field of `AnalogComBus` and `DigitalComBus`
 *   alongside `ChanLayer`. Does NOT replace `layer` — `layer` is still
 *   used by `_layer_ok()` for write protection (audit A6 confirmed it is
 *   functionally required). `direction` is a *data* field, not an
 *   access-control field.
 */
enum class Direction : uint8_t {
    None     = 0,
    Uplink   = 1,
    Downlink = 2,
    Both     = 3
};


// =============================================================================
// CHANNEL STRUCTS
// =============================================================================

/**
 * @brief Analog ComBus channel descriptor.
 */
typedef struct {
  const char* infoName;                            ///< Short description for debugging and dashboards
  uint16_t    value;                               ///< Current channel value (0‑65535)
  ChanLayer   layer       = ChanLayer::SYSTEM;     ///< Propagation layer — see ChanLayer
  Direction   direction   = Direction::None;       ///< Wire direction — added by A6.1
} AnalogComBus;

/**
 * @brief Digital ComBus channel descriptor.
 */
typedef struct {
  const char* infoName;                            ///< Short description for debugging and dashboards
  bool        value;                               ///< Current channel state (true/false)
  ChanLayer   layer       = ChanLayer::SYSTEM;     ///< Propagation layer — see ChanLayer
  Direction   direction   = Direction::None;       ///< Wire direction — added by A6.1
} DigitalComBus;

/**
 * @brief Main ComBus structure.
 *
 * @details Central shared register containing the current state of the system.
 *   All modules read from and write to this structure to collaborate without
 *   direct dependencies.
 */
typedef struct {
    // --- Core state ---
  // RL3: runLevel + runLevelLayer removed — runLevel is now a plain analog channel
  //       (RUNLEVEL, scope LOCAL, see runlevel.cb). Read/write it directly via the
  //       generic analog accessor (combus_set_analog / analogBus[RUNLEVEL].value).
  //       This is the same pattern as any other LOCAL analog channel (e.g. THROTTLE_STICK).

    // --- Input drive flag ---
  bool        isNotDrived = true;          ///< True when NO physical input source refreshed the bus this cycle (FS1 — inverted semantics, see RL0 audit)
                                            ///< Pre‑set by sys_manager_reset() each loop (failsafe-by-default); cleared by each active source.
                                            ///< Replaces the historical `isDrived` flag. Semantics inverted to match the Failsafe aggregator convention (true = fault).

    // --- Transport ---
  uint32_t    lastFrameMs = 0;             ///< millis() timestamp of the last successful combus_frame_apply

    // --- Channel arrays ---
  AnalogComBus*  analogBus;                ///< Analog channel array
  DigitalComBus* digitalBus;               ///< Digital channel array
  uint32_t       analogBusMaxVal;          ///< Maximum analog channel value (2¹⁶‑1 = 65535)
} ComBus;

// EOF combus_struct.h