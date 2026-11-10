/*!****************************************************************************
 * @file  remote_links.h
 * @brief Links of the remote to the machines it drives.
 *
 * @details One link per entry of kDrivenMachines (driven_machines.h), opened at init
 *   and run once per loop: receive, watch, then send the inputs.
 *******************************************************************************///
#pragma once

#include <stdint.h>

#include <core/system/combus/protocol/link/combus_links.h>   // ComLinkRuntime



	/// Open one link per driven machine and put them under watch; returns the number started.
uint8_t remote_links_init( void );


	/// One cycle of the links: receive, watch, send the inputs.
void remote_links_update( void );


	/// Storage of a link, for the tests and the diagnostics; nullptr out of range.
ComLinkRuntime* remote_links_runtime( uint8_t idx );

// EOF remote_links.h
