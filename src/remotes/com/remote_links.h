/*!****************************************************************************
 * @file  remote_links.h
 * @brief Links of the remote to the machines it drives (chantier 4, embryo).
 *******************************************************************************///
#pragma once

#include <stdint.h>

#include <core/system/combus/protocol/link/combus_links.h>   // ComLinkRuntime


/**
 * @brief Open one link per driven machine (kDrivenMachines) and put them under watch.
 *
 * @return the number of links started.
 */

uint8_t remote_links_init( void );


/**
 * @brief One cycle of the links: receive, watch, send (the inputs).
 */

void remote_links_update( void );


/**
 * @brief Storage of a link (tests, diagnostics); nullptr out of range.
 */

ComLinkRuntime* remote_links_runtime( uint8_t idx );

// EOF remote_links.h
