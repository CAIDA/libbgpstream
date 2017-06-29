/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BGPSTREAM_TRANSPORT_H
#define __BGPSTREAM_TRANSPORT_H

#include "bgpstream_resource.h"


/** Generic interface to specific data transport modules */
typedef struct bgpstream_transport bgpstream_transport_t;


/** Create a transport handler for the given resource
 *
 * @param res           pointer to a resource
 * @return pointer to a transport module instance if successful, NULL otherwise
 */
bgpstream_transport_t *
bgpstream_transport_create(bgpstream_resource_t *res);

/** Read from the given transport handler
 *
 * @param transport     pointer to a transport handler to read from
 * @return the number of bytes read if successful, -1 otherwise
 */
int64_t bgpstream_transport_read(bgpstream_transport_t *transport,
                                 uint8_t *buffer, int64_t len);

/** Shutdown and destroy the given transport handler
 *
 * @param transport     pointer to a transport handler to destroy
 */
void bgpstream_transport_destroy(bgpstream_transport_t *transport);

#endif /* __BGPSTREAM_TRANSPORT_H */
