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

#ifndef __BGPSTREAM_FORMAT_H
#define __BGPSTREAM_FORMAT_H

#include "bgpstream_resource.h"


/** Generic interface to specific data format modules */
typedef struct bgpstream_format bgpstream_format_t;

/** Create a format handler for the given resource
 *
 * @param res           pointer to a resource
 * @return pointer to a format module instance if successful, NULL otherwise
 */
bgpstream_format_t *
bgpstream_format_create(bgpstream_resource_t *res);

/** Get the next record from this format instance
 *
 * @param format        pointer to the format object to use
 * @param[out] record   set to point to a populated record instance
 * @return 1 if a record was read successfully, 0 if there is nothing more to
 * read, -1 if an error occurred.
 */
int bgpstream_format_get_next_record(bgpstream_format_t *format,
                                     bgpstream_record_t **record);

/** Destroy the given format module
 *
 * @param format        pointer to the format instance to destroy
 */
void bgpstream_format_destroy(bgpstream_format_t *format);


#endif /* __BGPSTREAM_FORMAT_H */
