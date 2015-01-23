/*
 * bgp-common
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgp-common.
 *
 * bgp-common is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgp-common is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgp-common.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef _BL_ID_SET_H
#define _BL_ID_SET_H

#include "bl_bgp_utils.h"


typedef struct bl_id_set_t bl_id_set_t;

 
/** Allocate memory for a strucure that maintains
 *  unique set of ids.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
bl_id_set_t *bl_id_set_create();

/** Insert a new id into the id set.
 *
 * @param id_set a pointer to the id set
 * @param id id to insert in the table
 * @return 1 if an id has been inserted, 0 if it already existed
 */
int bl_id_set_insert(bl_id_set_t *id_set, uint32_t id);

// TODO: documentation
int bl_id_set_exists(bl_id_set_t *id_set, uint32_t id);

/** Empty the id set.
 *
 * @param id_set a pointer to the id set
 */
void bl_id_set_reset(bl_id_set_t *id_set);

/** Get the size of the set.
 *
 * @param as_set pointer to the id set
 * @return the size of the id set
 */
int bl_id_set_size(bl_id_set_t *id_set);

/** Get the merge of the set.
 *  @param union_set pointer to the id set that will include the merge
 *  @param part_set pointer to the id set that will be merged with the union_set
 */
void bl_id_set_merge(bl_id_set_t *union_set, bl_id_set_t *part_set);

/** Deallocate memory for the AS table.
 *
 * @param id_set a pointer to the AS set
 */
void bl_id_set_destroy(bl_id_set_t *id_set);

#endif /* _BL_ID_SET_H */

