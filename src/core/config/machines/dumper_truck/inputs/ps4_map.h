/******************************************************************************
 * @file  ps4_map.h
 * @brief Mapper of the PS4 source for a dumper truck: the table of the type.
 *
 * @details The default table, for every dumper truck instance: the channels of the
 *   PS4 source (ps4.cb) to the inputs of the type (input.cb, KEY_BTN, the drive mode
 *   buttons).  An instance replaces it whole by defining ps4_instance_map() in
 *   machines/config/machines/<instance>/inputs/; no fusion.
 *   Design: doc/WIP - Local remote.md §5.1.
 *****************************************************************************/
#pragma once

#if defined(HAS_REMOTE_PS4)

#include <core/system/input/remote_map.h>   // RemoteMapTable


	/// The table of the type.
extern const RemoteMapTable kPs4TypeMap;


/**
 * @brief The table of the instance, if it declares one.
 *
 * @details Weak here, returning nullptr: an instance that customises the mapper
 *   defines it, and its table replaces the one of the type.
 *
 * @return The table of the instance; nullptr = the table of the type.
 */

const RemoteMapTable* ps4_instance_map( void );

#endif  // HAS_REMOTE_PS4

// EOF ps4_map.h
