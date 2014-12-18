/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __BGPWATCHER_VIEW_H
#define __BGPWATCHER_VIEW_H

#include "bl_peersign_map.h"

/** @file
 *
 * @brief Header file that exposes the public interface of bgpwatcher view.
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

typedef struct bgpwatcher_view bgpwatcher_view_t;

/** @} */


/** Create a new BGP View
 *
 * A BGP View holds a snapshot of the aggregated prefix information.
 * Basically, it maps from prefix -> peers -> prefix info
 *
 * @param peer_table    pointer to an external peer table, NULL to create an
 *                      internal table
 * @return a pointer to the view if successful, NULL otherwise
 */
bgpwatcher_view_t *bgpwatcher_view_create(bl_peersign_map_t *peer_table);

/** @todo create a nice high-level api for accessing information in the view */

/** Destroy the given BGP Watcher View
 *
 * @param view          pointer to the view to destroy
 */
void bgpwatcher_view_destroy(bgpwatcher_view_t *view);

/** Empty a view
 *
 * @param view          view to clear
 *
 * This does not actually free any memory, it just marks prefix and peers as
 * dirty so that future inserts can re-use the memory allocation. It does *not*
 * clear the peersigns table.
 */
void bgpwatcher_view_clear(bgpwatcher_view_t *view);

/** Dump the given BGP View to stdout
 *
 * @param view        pointer to a view structure
 */
void bgpwatcher_view_dump(bgpwatcher_view_t *view);

/** Get the total number of IPv4 prefixes in the view
 *
 * @param view          pointer to a view structure
 * @return the number of IPv4 prefixes in the view
 */
uint32_t bgpwatcher_view_v4size(bgpwatcher_view_t *view);

/** Get the total number of IPv6 prefixes in the view
 *
 * @param view          pointer to a view structure
 * @return the number of IPv6 prefixes in the view
 */
uint32_t bgpwatcher_view_v6size(bgpwatcher_view_t *view);

/** Get the total number of prefixes (v4+v6) in the view
 *
 * @param view          pointer to a view structure
 * @return the number of prefixes in the view
 */
uint32_t bgpwatcher_view_size(bgpwatcher_view_t *view);

/** Get the BGP time that the view represents
 *
 * @param view          pointer to a view structure
 * @return the time that the view represents
 */
uint32_t bgpwatcher_view_time(bgpwatcher_view_t *view);

#endif /* __BGPWATCHER_VIEW_H */
