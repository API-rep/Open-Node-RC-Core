/*!****************************************************************************
 * @file  combus_ids_remote.h
 * @brief Dumper-truck REMOTE-only channel IDs — generic vocabulary for ALL dumper-truck instances
 *
 * @details This file provides the REMOTE channel vocabulary for the dumper-truck
 *   vehicle class. It contains ONLY the channels that are transmitted over the
 *   wire to sound nodes and other remote receivers — common to ALL dumper-truck
 *   implementations (Volvo A60H Bruder, future dumper-trucks, etc.).
 *
 *   LOCAL and SYSTEM channels (specific to a concrete machine instance) are
 *   NOT included here — they live in the machine-specific combus_ids.h.
 *
 *   USAGE:
 *     - Generic dumper-truck code (core/system/*) → include this file,
 *       use AnalogComBusRemoteID/DigitalComBusRemoteID in scope global
 *     - Machine-specific code (volvo_A60H_bruder/*) → include machine's
 *       combus_ids.h, use AnalogComBusID/DigitalComBusID in scope global
 *
 *   ARCHITECTURE:
 *     - core/config/machines/dumper_truck/ = TYPE definition (generic)
 *     - src/machines/config/machines/volvo_A60H_bruder/ = INSTANCE (concrete)
 *     - This file is the ONLY source for Remote vocabulary in TYPE scope
 *
 *   IMPORTANT: Canal THROTTLE_BUS doit être ajouté au fragment
 *   combus_ids_remote_analog.inc s'il n'est pas déjà présent.
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
    // NOTE: THROTTLE_BUS should be added here if missing from the .inc
    #include "combus/combus_ids_remote_analog.inc"
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
    #include "combus/combus_ids_remote_digital.inc"
    CH_COUNT
};

// EOF combus_ids_remote.h