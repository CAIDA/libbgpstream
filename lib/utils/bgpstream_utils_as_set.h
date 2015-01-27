/*
 * bgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpstream.
 *
 * bgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef _BL_AS_SET_H
#define _BL_AS_SET_H

#include "bgpstream_utils.h"


typedef struct bl_as_storage_set_t bl_as_storage_set_t;

				   
/** Allocate memory for a strucure that maintains
 *  unique set of AS numbers.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
bl_as_storage_set_t *bl_as_storage_set_create();

/** Insert a new AS into the AS set.
 *
 * @param as_set a pointer to the AS set
 * @param as AS number to insert in the table
 * @return 1 if an AS number has been inserted, 0 if it already existed
 */
int bl_as_storage_set_insert(bl_as_storage_set_t *as_set, bl_as_storage_t as);

/** Empty the AS set.
 *
 * @param as_set a pointer to the AS set
 */
void bl_as_storage_set_reset(bl_as_storage_set_t *as_set);

int bl_as_storage_set_size(bl_as_storage_set_t *as_set);
void bl_as_storage_set_merge(bl_as_storage_set_t *union_set, bl_as_storage_set_t *part_set);


/** Deallocate memory for the AS table.
 *
 * @param as_set a pointer to the AS set
 */
void bl_as_storage_set_destroy(bl_as_storage_set_t *as_set);

#endif /* _BL_AS_SET_H */

