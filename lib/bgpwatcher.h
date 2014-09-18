/*
 * bgpwatcher
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#ifndef __BGPWATCHER_H
#define __BGPWATCHER_H

#include <bgpwatcher_common.h>

/** @file
 *
 * @brief Header file that exposes the public interface of bgpwatcher. For the
 * client API, see bgpwatcher_client.h
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

typedef struct bgpwatcher bgpwatcher_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** @} */

/** Initialize a new BGP Watcher instance
 *
 * @return a pointer to a bgpwatcher instance if successful, NULL if an error
 * occurred.
 */
bgpwatcher_t *bgpwatcher_init();

/** Start the given bgpwatcher instance
 *
 * @param watcher       pointer to a bgpwatcher instance to start
 * @return 0 if the watcher exited cleanly, -1 otherwise.
 *
 * To get more information about the exit state of bgpwatcher, call
 * bgpwatcher_perr()
 *
 * @note this function will also start up a bgpwatcher_server instance and begin
 * listening for bgpwatcher_client connections. As such, all server config
 * options must be set prior to calling this function
 */
int bgpwatcher_start(bgpwatcher_t *watcher);

/** Stop the given bgpwatcher instance at the next safe occasion.
 *
 * This is useful to initiate a clean shutdown if you are handling signals in
 * the program driving bgpwatcher. Call this from within your signal handler.
 *
 * @param watcher       pointer to the bgpwatcher instance to stop
 */
void bgpwatcher_stop(bgpwatcher_t *watcher);

/** Free the given bgpwatcher instance
 *
 * @param watcher       pointer to the bgpwatcher instance to free
 */
void bgpwatcher_free(bgpwatcher_t *watcher);

/** Get the error status for the given bgpwatcher instance
 *
 * @param watcher       pointer to a bgpwatcher instance to retrieve status for
 * @return an error status object that can be passed to bgpwatcher_perr etc
 */
bgpwatcher_err_t bgpwatcher_get_err(bgpwatcher_t *watcher);

#endif
