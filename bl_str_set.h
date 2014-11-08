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


#ifndef _BL_STR_SET_H
#define _BL_STR_SET_H

#include "khash.h"

/** set of unique strings
 *  this structure maintains a set of strings
 */

KHASH_INIT(bl_string_set, char*, char, 0,
	   kh_str_hash_func, kh_str_hash_equal);


typedef khash_t(bl_string_set) bl_string_set_t;
	    	    
/** Allocate memory for a strucure that maintains
 *  unique set of strings.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
bl_string_set_t *bl_string_set_create();

/** Insert a new string into the string set.
 *
 * @param string_set pointer to the string set
 * @param string_val the string to insert
 * @return 1 if a string has been inserted, 0 if it already existed
 */
int bl_string_set_insert(bl_string_set_t *string_set, char * string_val);

/** Remove a string from the set
 *
 * @param string_val pointer to the string set
 */
int bl_string_set_remove(bl_string_set_t *string_set, char * string_val);

/** Check whether a string exists in the set
 *
 * @param string_val pointer to the string set
 */
int bl_string_set_exists(bl_string_set_t *string_set, char * string_val);

/** Returns the number of unique strings in the set
 *
 * @param string_val pointer to the string set
 */
int bl_string_set_size(bl_string_set_t *string_set);

/** Empty the string set.
 *
 * @param string_set pointer to the string set
 */
void bl_string_set_reset(bl_string_set_t *string_set);

/** Deallocate memory for the IP prefix set
 *
 * @param as_set a pointer to the AS set
 */
void bl_string_set_destroy(bl_string_set_t *string_set);


#endif /* _BL_STR_SET_H */

