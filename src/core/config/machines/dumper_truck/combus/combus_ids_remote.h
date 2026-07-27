/*!****************************************************************************
 * @file  combus_ids.h
 * @brief Dumper-truck REMOTE-only channel IDs — generic vocabulary for ALL dumper-truck instances
 *
 * @details This file provides the REMOTE combus channels common over the dumper-truck
 *   vehicle class. It contains ONLY the channels that are transmitted over the combus
 *   remote layer.
 *
 *   LOCAL and SYSTEM channels (specific to a concrete machine instance) are
 *   NOT included here — they live in the machine-specific combus.h.
 *
 *   USAGE:
 *     - Generic dumper-truck code (inputs_map, lights, motion, sound)
 *       → include this file, use global scope (no namespace)
 *     - Machine-specific code (Volvo A60H Bruder processors)
 *       → include machine's combus.h, use global scope
 *
 *   WHY THIS EXISTS:
 *     - Separation of concerns: generic vs instance-specific
 *     - Reusability: new dumper-truck instances reuse REMOTE vocabulary
 *     - Compilation: generic core code compiles without machine dependencies
 *
 *   CHANNEL SELECTION CRITERIA:
 *     - Transmitted from/to remote nodes (typ. remote to machine connection)
 *     - Common to ALL dumper-truck implementations
 *     - No instance-specific behavior or hardware mapping
 *******************************************************************************///
#pragma once

#include <cstdint>

// =============================================================================
// 1. ANALOG REMOTE CHANNELS (shared by ALL dumper-truck instances)
// =============================================================================

/**
 * @brief Analog REMOTE channels shared by all dumper-truck instances.
 *
 * @details These channels are transmitted over the wire to sound nodes
 *   and other remote receivers. They represent the core vehicle state
 *   that is common to ALL dumper-trucks.
 */
enum class AnalogComBusRemoteID : uint8_t {
    #include "combus_ids_remote_analog.inc"
    CH_COUNT
};

// =============================================================================
// 2. DIGITAL REMOTE CHANNELS (shared by ALL dumper-truck instances)
// =============================================================================

/**
 * @brief Digital REMOTE channels shared by all dumper-truck instances.
 *
 * @details These channels are transmitted over the wire as packed bits.
 *   They represent boolean states common to ALL dumper-trucks.
 */
enum class DigitalComBusRemoteID : uint8_t {
    #include "combus_ids_remote_digital.inc"
    CH_COUNT
};

// EOF combus_ids.h