/******************************************************************************
 * @file dashboard_links.h
 * @brief ANSI terminal dashboard — links view (key '7').
 *
 * @details A home page listing the links under watch (name, scope, state,
 *   contract, mode, coupler and role, pairing of a coupled link) and the
 *   passive couplers; one detail page per link and per passive coupler,
 *   navigable as the other detail views.  Reads the snapshots exposed by
 *   combus_links (combus_links_view, combus_couplings_view), never its
 *   internal structures.
 *****************************************************************************/
#pragma once

#include <core/system/combus/combus_defs.h>   // ComBus

#ifdef DEBUG_DASHBOARD

void dashboard_links_register(const ComBus* bus);

#else // !DEBUG_DASHBOARD
inline void dashboard_links_register(const ComBus*) {}
#endif // DEBUG_DASHBOARD

// EOF dashboard_links.h
